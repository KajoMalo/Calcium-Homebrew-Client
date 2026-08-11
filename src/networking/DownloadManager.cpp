#include "DownloadManager.hpp"
#include "../logging/Logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace calcium::networking {

static constexpr std::string_view TAG = "DownloadManager";

// ─── Construction / destruction ───────────────────────────────────────────────

DownloadManager::DownloadManager(std::shared_ptr<IHttpClient> http_client)
    : m_http(std::move(http_client))
{
    if (!m_http) throw std::invalid_argument("DownloadManager: http_client must not be null");
    m_worker = std::thread(&DownloadManager::worker_loop, this);
}

DownloadManager::~DownloadManager() {
    cancel_all();
    {
        std::lock_guard lock{m_mutex};
        m_stop.store(true);
    }
    m_cv.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

// ─── Public API ───────────────────────────────────────────────────────────────

void DownloadManager::set_callbacks(DownloadCallbacks callbacks) {
    std::lock_guard lock{m_mutex};
    m_callbacks = std::move(callbacks);
}

bool DownloadManager::enqueue(std::string id,
                               std::string url,
                               std::string dest_path,
                               std::string display_name) {
    std::lock_guard lock{m_mutex};

    // Reject duplicate ids.
    auto already_queued = std::any_of(m_queue.begin(), m_queue.end(),
        [&](const DownloadItem& i) { return i.id == id; });
    if (already_queued) {
        logging::Logger::instance().warning(TAG, "Already queued: " + id);
        return false;
    }
    if (m_active && m_active->id == id) {
        logging::Logger::instance().warning(TAG, "Already downloading: " + id);
        return false;
    }

    DownloadItem item;
    item.id           = std::move(id);
    item.url          = std::move(url);
    item.dest_path    = std::move(dest_path);
    item.display_name = display_name.empty() ? item.id : std::move(display_name);
    item.status       = DownloadStatus::Queued;

    logging::Logger::instance().info(TAG, "Enqueued: " + item.id);
    m_queue.push_back(std::move(item));
    m_cv.notify_one();
    return true;
}

bool DownloadManager::cancel(const std::string& id) {
    std::lock_guard lock{m_mutex};

    // Cancel if still queued.
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->id == id) {
            it->status = DownloadStatus::Cancelled;
            logging::Logger::instance().info(TAG, "Cancelled (queued): " + id);
            DownloadItem item = std::move(*it);
            m_queue.erase(it);
            m_history.push_back(item);
            if (m_callbacks.on_cancelled) m_callbacks.on_cancelled(item);
            return true;
        }
    }

    // Cancel if currently downloading.
    if (m_active && m_active->id == id) {
        m_http->cancel();
        logging::Logger::instance().info(TAG, "Cancellation requested for active: " + id);
        return true;
    }

    return false;
}

void DownloadManager::cancel_all() {
    std::lock_guard lock{m_mutex};
    m_http->cancel();
    for (auto& item : m_queue) {
        item.status = DownloadStatus::Cancelled;
        m_history.push_back(item);
        if (m_callbacks.on_cancelled) m_callbacks.on_cancelled(item);
    }
    m_queue.clear();
}

std::vector<DownloadItem> DownloadManager::all_items() const {
    std::lock_guard lock{m_mutex};
    std::vector<DownloadItem> result;
    if (m_active) result.push_back(*m_active);
    for (const auto& item : m_queue)   result.push_back(item);
    for (const auto& item : m_history) result.push_back(item);
    return result;
}

std::optional<DownloadItem> DownloadManager::find(const std::string& id) const {
    std::lock_guard lock{m_mutex};
    if (m_active && m_active->id == id) return *m_active;
    for (const auto& item : m_queue)   if (item.id == id) return item;
    for (const auto& item : m_history) if (item.id == id) return item;
    return std::nullopt;
}

bool DownloadManager::is_downloading() const {
    std::lock_guard lock{m_mutex};
    return m_active.has_value();
}

std::size_t DownloadManager::queue_size() const {
    std::lock_guard lock{m_mutex};
    return m_queue.size();
}

// ─── Worker thread ────────────────────────────────────────────────────────────

void DownloadManager::worker_loop() {
    while (true) {
        DownloadItem item;
        {
            std::unique_lock lock{m_mutex};
            m_cv.wait(lock, [this] {
                return m_stop.load() || !m_queue.empty();
            });
            if (m_stop.load() && m_queue.empty()) break;
            if (m_queue.empty()) continue;

            item = std::move(m_queue.front());
            m_queue.pop_front();
            item.status = DownloadStatus::Downloading;
            m_active = item;
        }

        process_item(item);

        {
            std::lock_guard lock{m_mutex};
            m_history.push_back(item);
            m_active.reset();
        }
    }
}

void DownloadManager::process_item(DownloadItem& item) {
    logging::Logger::instance().info(TAG,
        "Starting download: " + item.id + " from " + item.url);

    bool success = false;

    for (int attempt = 0; attempt <= m_max_retries; ++attempt) {
        if (m_http->is_cancelled()) {
            item.status        = DownloadStatus::Cancelled;
            item.error_message = "Cancelled by user";
            logging::Logger::instance().info(TAG, "Cancelled: " + item.id);
            if (m_callbacks.on_cancelled) m_callbacks.on_cancelled(item);
            return;
        }

        if (attempt > 0) {
            logging::Logger::instance().info(TAG,
                "Retry " + std::to_string(attempt) + "/" + std::to_string(m_max_retries) +
                " for: " + item.id);
            // Brief back-off before retry.
            std::this_thread::sleep_for(std::chrono::seconds(attempt));
        }

        auto progress_cb = [this, &item](uint64_t received, uint64_t total) {
            item.bytes_received = received;
            item.bytes_total    = total;
            update_item(item.id, [&](DownloadItem& stored) {
                stored.bytes_received = received;
                stored.bytes_total    = total;
            });
            if (m_callbacks.on_progress) m_callbacks.on_progress(item);
        };

        success = m_http->download(item.url, item.dest_path, progress_cb, m_http_options);

        if (success) break;
        if (m_http->is_cancelled()) {
            item.status        = DownloadStatus::Cancelled;
            item.error_message = "Cancelled during download";
            if (m_callbacks.on_cancelled) m_callbacks.on_cancelled(item);
            return;
        }
    }

    if (success) {
        item.status = DownloadStatus::Completed;
        logging::Logger::instance().info(TAG, "Completed: " + item.id);
        if (m_callbacks.on_completed) m_callbacks.on_completed(item);
    } else {
        item.status        = DownloadStatus::Failed;
        item.error_message = "Download failed after " +
                             std::to_string(m_max_retries) + " retries";
        logging::Logger::instance().error(TAG,
            "Failed: " + item.id + " — " + item.error_message);
        if (m_callbacks.on_failed) m_callbacks.on_failed(item);
    }
}

void DownloadManager::update_item(const std::string& id,
                                   std::function<void(DownloadItem&)> mutator) {
    // Update the active item snapshot so queries reflect live progress.
    std::lock_guard lock{m_mutex};
    if (m_active && m_active->id == id) {
        mutator(*m_active);
    }
}

} // namespace calcium::networking
