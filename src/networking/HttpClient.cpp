#include "HttpClient.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <stdexcept>
#include <mutex>
#include <atomic>

#ifdef CALCIUM_USE_CURL
#  include <curl/curl.h>
#endif

namespace calcium::networking {

static constexpr std::string_view TAG = "HttpClient";

// ─── libcurl global lifecycle ─────────────────────────────────────────────────
#ifdef CALCIUM_USE_CURL

static std::mutex         g_curl_mutex;
static std::atomic<int>   g_curl_refcount{0};

void HttpClient::global_init() {
    std::lock_guard lock{g_curl_mutex};
    if (g_curl_refcount.fetch_add(1) == 0) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        logging::Logger::instance().debug(TAG, "curl_global_init called.");
    }
}

void HttpClient::global_cleanup() {
    std::lock_guard lock{g_curl_mutex};
    if (g_curl_refcount.fetch_sub(1) == 1) {
        curl_global_cleanup();
        logging::Logger::instance().debug(TAG, "curl_global_cleanup called.");
    }
}

// ─── curl write callbacks ─────────────────────────────────────────────────────

static size_t write_to_string(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

static size_t write_to_file(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* file = static_cast<std::ofstream*>(userdata);
    file->write(ptr, static_cast<std::streamsize>(size * nmemb));
    return file->good() ? size * nmemb : 0;
}

struct ProgressData {
    ProgressCallback          callback;
    std::atomic<bool>*        cancelled;
};

static int progress_callback(void* clientp,
                              curl_off_t dltotal, curl_off_t dlnow,
                              curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    auto* data = static_cast<ProgressData*>(clientp);
    if (data->cancelled->load()) return 1; // non-zero aborts the transfer
    if (data->callback) {
        data->callback(
            static_cast<uint64_t>(dlnow),
            static_cast<uint64_t>(dltotal)
        );
    }
    return 0;
}

// ─── Shared curl setup helper ─────────────────────────────────────────────────

static void apply_options(CURL* curl, const HttpOptions& opts) {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       static_cast<long>(opts.timeout_secs));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, opts.follow_redirect ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CalciumClient/1.0");

    // Custom headers
    if (!opts.headers.empty()) {
        struct curl_slist* list = nullptr;
        for (const auto& [k, v] : opts.headers) {
            list = curl_slist_append(list, (k + ": " + v).c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        // Note: the list lifetime is handled by the caller for simplicity;
        // in a full production build we'd use a RAII wrapper.
    }
}

#endif // CALCIUM_USE_CURL

// ─── HttpClient ───────────────────────────────────────────────────────────────

HttpClient::HttpClient() {
#ifdef CALCIUM_USE_CURL
    global_init();
#endif
}

HttpClient::~HttpClient() {
#ifdef CALCIUM_USE_CURL
    global_cleanup();
#endif
}

void HttpClient::cancel() {
    m_cancelled.store(true);
}

bool HttpClient::is_cancelled() const {
    return m_cancelled.load();
}

// ─── GET ──────────────────────────────────────────────────────────────────────

HttpResponse HttpClient::get(const std::string& url, const HttpOptions& options) {
    m_cancelled.store(false);

#ifdef CALCIUM_USE_CURL
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error_message = "curl_easy_init failed";
        return resp;
    }

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    apply_options(curl, options);

    ProgressData pd{nullptr, &m_cancelled};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &pd);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

    int attempt = 0;
    CURLcode res = CURLE_OK;
    do {
        body.clear();
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) break;
        if (m_cancelled.load()) break;
        logging::Logger::instance().warning(TAG,
            "GET attempt " + std::to_string(attempt + 1) + " failed: " +
            curl_easy_strerror(res));
        ++attempt;
    } while (attempt < options.max_retries);

    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        resp.status_code = static_cast<int>(code);
        resp.body = std::move(body);
    } else {
        resp.error_message = curl_easy_strerror(res);
    }

    curl_easy_cleanup(curl);
    return resp;

#else
    // Stub: no HTTP stack available.
    (void)url; (void)options;
    HttpResponse resp;
    resp.error_message = "HTTP client not available (built without curl)";
    logging::Logger::instance().error(TAG, resp.error_message);
    return resp;
#endif
}

// ─── Download ─────────────────────────────────────────────────────────────────

bool HttpClient::download(const std::string& url,
                          const std::string& dest_path,
                          ProgressCallback   on_progress,
                          const HttpOptions& options) {
    m_cancelled.store(false);

#ifdef CALCIUM_USE_CURL
    // Ensure parent directory exists.
    {
        std::filesystem::path p{dest_path};
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
    }

    std::ofstream file(dest_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open destination file: " + dest_path);
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        logging::Logger::instance().error(TAG, "curl_easy_init failed");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    apply_options(curl, options);

    ProgressData pd{std::move(on_progress), &m_cancelled};
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &pd);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

    int attempt = 0;
    CURLcode res = CURLE_OK;
    do {
        if (attempt > 0) {
            // Seek back to the start of the file for a clean retry.
            file.seekp(0);
            file.clear();
        }
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) break;
        if (m_cancelled.load()) break;
        logging::Logger::instance().warning(TAG,
            "Download attempt " + std::to_string(attempt + 1) + " failed: " +
            curl_easy_strerror(res));
        ++attempt;
    } while (attempt < options.max_retries);

    bool success = false;
    if (res == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        if (code >= 200 && code < 300) {
            success = true;
            logging::Logger::instance().info(TAG, "Downloaded: " + url);
        } else {
            logging::Logger::instance().error(TAG,
                "Download HTTP error " + std::to_string(code) + " for " + url);
        }
    } else if (m_cancelled.load()) {
        logging::Logger::instance().info(TAG, "Download cancelled: " + url);
    } else {
        logging::Logger::instance().error(TAG,
            "Download failed: " + std::string(curl_easy_strerror(res)));
    }

    curl_easy_cleanup(curl);
    file.close();

    if (!success) {
        // Remove partially-written file on failure/cancellation.
        std::error_code ec;
        std::filesystem::remove(dest_path, ec);
    }
    return success;

#else
    (void)url; (void)dest_path; (void)on_progress; (void)options;
    logging::Logger::instance().error(TAG, "Download not available (built without curl)");
    return false;
#endif
}

} // namespace calcium::networking
