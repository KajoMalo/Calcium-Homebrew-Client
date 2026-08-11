#include "Repository.hpp"
#include "MetadataParser.hpp"
#include "../logging/Logger.hpp"

#include <fstream>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace calcium::repository {

static constexpr std::string_view TAG = "Repository";

static std::string current_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
#if defined(_WIN32)
    gmtime_s(&buf, &tt);
#else
    gmtime_r(&tt, &buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// ─── Construction ─────────────────────────────────────────────────────────────

Repository::Repository(std::string id,
                       std::string name,
                       std::string url,
                       std::shared_ptr<networking::IHttpClient> http_client)
    : m_id(std::move(id))
    , m_name(std::move(name))
    , m_url(std::move(url))
    , m_http(std::move(http_client))
{}

// ─── Refresh ──────────────────────────────────────────────────────────────────

RepoResult Repository::refresh() {
    logging::Logger::instance().info(TAG,
        "Refreshing repository '" + m_name + "' from " + m_url);

    // ── file:// scheme — load directly from disk (mock / local repos) ────
    if (m_url.rfind("file://", 0) == 0) {
        std::string path = m_url.substr(7); // strip "file://"
        std::ifstream f(path);
        if (!f.is_open()) {
            return RepoResult::fail("Cannot open local repository file: " + path);
        }
        std::string body(std::istreambuf_iterator<char>(f),
                         std::istreambuf_iterator<char>());
        return load_from_json(body);
    }

    // ── HTTP/HTTPS ────────────────────────────────────────────────────────
    if (!m_http) {
        return RepoResult::fail("No HTTP client configured for repository '" + m_id + "'");
    }

    networking::HttpOptions opts;
    opts.timeout_secs = 30;

    auto response = m_http->get(m_url, opts);

    if (response.failed()) {
        return RepoResult::fail("Network error fetching '" + m_url + "': " + response.error_message);
    }
    if (!response.ok()) {
        return RepoResult::fail("HTTP " + std::to_string(response.status_code) +
                                " fetching repository index: " + m_url);
    }

    return load_from_json(response.body);
}

RepoResult Repository::load_from_json(const std::string& json_text) {
    auto result = MetadataParser::parse_index(json_text);
    if (!result.ok()) {
        return RepoResult::fail("Failed to parse index for '" + m_id + "': " + result.error);
    }

    // Stamp every app with the source repo id.
    for (auto& app : *result.value) {
        app.repo_id = m_id;
    }

    {
        std::lock_guard lock{m_mutex};
        m_apps         = std::move(*result.value);
        m_last_updated = current_iso8601();
        m_loaded       = true;
    }

    logging::Logger::instance().info(TAG,
        "Repository '" + m_name + "' loaded " +
        std::to_string(m_apps.size()) + " app(s).");

    return RepoResult::ok();
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool Repository::is_loaded() const {
    std::lock_guard lock{m_mutex};
    return m_loaded;
}

const std::vector<AppMetadata>& Repository::apps() const {
    // Caller must not call this from multiple threads while refresh() is running.
    return m_apps;
}

std::optional<AppMetadata> Repository::find_app(const std::string& app_id) const {
    std::lock_guard lock{m_mutex};
    for (const auto& app : m_apps) {
        if (app.id == app_id) return app;
    }
    return std::nullopt;
}

const std::string& Repository::last_updated() const {
    std::lock_guard lock{m_mutex};
    return m_last_updated;
}

} // namespace calcium::repository
