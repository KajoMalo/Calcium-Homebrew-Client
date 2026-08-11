#pragma once

#include "IHttpClient.hpp"

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace calcium::networking {

/// Status of a single download item.
enum class DownloadStatus {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled,
};

inline constexpr std::string_view to_string(DownloadStatus s) {
    switch (s) {
        case DownloadStatus::Queued:      return "Queued";
        case DownloadStatus::Downloading: return "Downloading";
        case DownloadStatus::Paused:      return "Paused";
        case DownloadStatus::Completed:   return "Completed";
        case DownloadStatus::Failed:      return "Failed";
        case DownloadStatus::Cancelled:   return "Cancelled";
    }
    return "Unknown";
}

/// All state associated with one queued download.
struct DownloadItem {
    std::string id;           ///< Unique identifier (typically app_id).
    std::string url;
    std::string dest_path;
    std::string display_name; ///< Shown in the downloads UI.

    DownloadStatus status        = DownloadStatus::Queued;
    uint64_t       bytes_received = 0;
    uint64_t       bytes_total    = 0;
    std::string    error_message;

    /// Returns 0.0–1.0 progress fraction, or 0 if total unknown.
    float progress() const {
        if (bytes_total == 0) return 0.0f;
        return static_cast<float>(bytes_received) / static_cast<float>(bytes_total);
    }
};

/// Callbacks the owner registers to react to state changes.
struct DownloadCallbacks {
    std::function<void(const DownloadItem&)> on_progress;   ///< Called frequently during transfer.
    std::function<void(const DownloadItem&)> on_completed;  ///< Called once on success.
    std::function<void(const DownloadItem&)> on_failed;     ///< Called once on terminal failure.
    std::function<void(const DownloadItem&)> on_cancelled;
};

/// Manages a sequential download queue with progress reporting,
/// cancellation, and per-item retry support.
///
/// Downloads execute one at a time on a dedicated worker thread so the UI
/// thread is never blocked.
class DownloadManager {
public:
    explicit DownloadManager(std::shared_ptr<IHttpClient> http_client);
    ~DownloadManager();

    // Non-copyable.
    DownloadManager(const DownloadManager&)            = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    /// Register callbacks for all download events.
    void set_callbacks(DownloadCallbacks callbacks);

    /// Add a download to the queue. Returns false if id is already queued.
    bool enqueue(std::string id,
                 std::string url,
                 std::string dest_path,
                 std::string display_name = "");

    /// Cancel the download with the given id (queued or in-progress).
    bool cancel(const std::string& id);

    /// Cancel all downloads.
    void cancel_all();

    // ── Queries ────────────────────────────────────────────────────────────

    /// Snapshot of all items (thread-safe copy).
    std::vector<DownloadItem> all_items() const;

    /// Find a specific item by id.
    std::optional<DownloadItem> find(const std::string& id) const;

    bool is_downloading() const;
    std::size_t queue_size() const;

    /// Set the maximum number of retry attempts per item (default: 3).
    void set_max_retries(int retries) { m_max_retries = retries; }

    /// Set the HTTP options applied to every download.
    void set_http_options(HttpOptions opts) { m_http_options = std::move(opts); }

private:
    void worker_loop();
    void process_item(DownloadItem& item);
    void update_item(const std::string& id,
                     std::function<void(DownloadItem&)> mutator);

    std::shared_ptr<IHttpClient>       m_http;
    DownloadCallbacks                  m_callbacks;
    HttpOptions                        m_http_options;

    mutable std::mutex                 m_mutex;
    std::condition_variable            m_cv;
    std::deque<DownloadItem>           m_queue;     ///< Items not yet started.
    std::vector<DownloadItem>          m_history;   ///< Completed/failed/cancelled.
    std::optional<DownloadItem>        m_active;    ///< Currently downloading.

    std::thread                        m_worker;
    std::atomic<bool>                  m_stop{false};
    int                                m_max_retries{3};
};

} // namespace calcium::networking
