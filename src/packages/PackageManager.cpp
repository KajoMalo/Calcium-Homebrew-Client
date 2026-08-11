#include "PackageManager.hpp"
#include "../logging/Logger.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace calcium::packages {

static constexpr std::string_view TAG = "PackageManager";

static std::string now_iso8601() {
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

PackageManager::PackageManager(
    std::shared_ptr<networking::IHttpClient>   http_client,
    std::shared_ptr<filesystem::IFilesystem>   fs,
    std::shared_ptr<config::InstalledDatabase> db,
    const config::AppConfig&                   cfg)
    : m_http(std::move(http_client))
    , m_fs(std::move(fs))
    , m_db(std::move(db))
    , m_cfg(cfg)
    , m_installer(m_fs, cfg.install_dir)
{}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void PackageManager::emit(ProgressCallback& cb, const PackageProgress& pp) {
    if (cb) cb(pp);
}

void PackageManager::cancel() {
    m_cancel_requested.store(true);
    m_http->cancel();
    m_installer.cancel();
}

bool PackageManager::is_busy() const {
    return m_busy.load();
}

// ─── Install pipeline ─────────────────────────────────────────────────────────

PackageResult PackageManager::install(const repository::AppMetadata& meta,
                                       ProgressCallback on_progress) {
    if (m_busy.exchange(true)) {
        return PackageResult::fail(meta.id, "Another operation is already in progress.");
    }
    m_cancel_requested.store(false);

    // RAII busy flag reset.
    struct BusyGuard {
        std::atomic<bool>& flag;
        ~BusyGuard() { flag.store(false); }
    } guard{m_busy};

    logging::Logger::instance().info(TAG, "Install requested: " + meta.id);

    // ── Check storage space ───────────────────────────────────────────────
    if (meta.download_size > 0 || meta.installed_size > 0) {
        auto needed = std::max(meta.download_size, meta.installed_size);
        auto space  = m_fs->space(m_cfg.download_dir);
        if (space.available > 0 && space.available < needed + (50 * 1024 * 1024)) {
            return PackageResult::fail(meta.id,
                "Insufficient storage space. Need " +
                std::to_string(needed / (1024 * 1024)) + " MB, available " +
                std::to_string(space.available / (1024 * 1024)) + " MB.");
        }
    }

    // ── Download ──────────────────────────────────────────────────────────
    m_fs->create_directories(m_cfg.download_dir);
    const auto pkg_path = std::filesystem::path(m_cfg.download_dir) /
                          (meta.id + "-" + meta.version + ".pkg");

    {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Downloading;
        pp.progress       = 0.0f;
        pp.status_message = "Downloading " + meta.name + "...";
        emit(on_progress, pp);
    }

    networking::HttpOptions http_opts;
    http_opts.timeout_secs = m_cfg.download_timeout_secs;
    http_opts.max_retries  = m_cfg.download_max_retries;

    auto dl_progress = [&](uint64_t received, uint64_t total) {
        if (!on_progress) return;
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Downloading;
        pp.bytes_received = received;
        pp.bytes_total    = total;
        pp.progress       = (total > 0)
            ? static_cast<float>(received) / static_cast<float>(total) * 0.6f
            : 0.3f;
        pp.status_message = "Downloading... " +
            std::to_string(received / 1024) + " / " +
            (total > 0 ? std::to_string(total / 1024) : "?") + " KB";
        emit(on_progress, pp);
    };

    bool downloaded = m_http->download(meta.download_url, pkg_path.string(),
                                        dl_progress, http_opts);

    if (m_cancel_requested.load()) {
        m_fs->remove(pkg_path);
        return PackageResult::fail(meta.id, "Cancelled during download.");
    }
    if (!downloaded) {
        return PackageResult::fail(meta.id, "Download failed for: " + meta.download_url);
    }

    // ── Verify ────────────────────────────────────────────────────────────
    {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Verifying;
        pp.progress       = 0.62f;
        pp.status_message = "Verifying package integrity...";
        emit(on_progress, pp);
    }

    if (m_cfg.verify_hashes && !meta.sha256.empty()) {
        auto vr = PackageVerifier::verify(pkg_path, meta.sha256);
        if (!vr.passed) {
            m_fs->remove(pkg_path);
            std::string err = vr.error.empty()
                ? "SHA-256 mismatch: expected " + meta.sha256 +
                  " got " + vr.computed_hash
                : vr.error;
            return PackageResult::fail(meta.id, err);
        }
    } else if (!m_cfg.verify_hashes) {
        logging::Logger::instance().warning(TAG, "Hash verification disabled in config.");
    }

    if (m_cancel_requested.load()) {
        m_fs->remove(pkg_path);
        return PackageResult::fail(meta.id, "Cancelled during verification.");
    }

    // ── Install ───────────────────────────────────────────────────────────
    {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Installing;
        pp.progress       = 0.65f;
        pp.status_message = "Installing...";
        emit(on_progress, pp);
    }

    // Forward install progress, scaling it to the 65–95% window.
    auto install_progress = [&](const PackageProgress& inner) {
        if (!on_progress) return;
        PackageProgress pp = inner;
        pp.progress = 0.65f + inner.progress * 0.30f;
        emit(on_progress, pp);
    };

    std::string install_path = m_installer.install(pkg_path, meta, install_progress);

    // Remove the downloaded package file after install attempt.
    m_fs->remove(pkg_path);

    if (install_path.empty()) {
        return PackageResult::fail(meta.id, "Package extraction/installation failed.");
    }

    // ── Register in database ──────────────────────────────────────────────
    config::InstalledRecord record;
    record.app_id         = meta.id;
    record.name           = meta.name;
    record.version        = meta.version;
    record.install_path   = install_path;
    record.content_id     = meta.content_id;
    record.repo_id        = meta.repo_id;
    record.installed_size = meta.installed_size;
    record.installed_at   = now_iso8601();

    if (!m_db->upsert(std::move(record))) {
        logging::Logger::instance().warning(TAG,
            "Installed but failed to persist record for: " + meta.id);
    }

    {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Completed;
        pp.progress       = 1.0f;
        pp.status_message = meta.name + " installed successfully.";
        emit(on_progress, pp);
    }

    logging::Logger::instance().info(TAG, "Install complete: " + meta.id);
    return PackageResult::ok(meta.id);
}

// ─── Uninstall pipeline ───────────────────────────────────────────────────────

PackageResult PackageManager::uninstall(const std::string& app_id,
                                         ProgressCallback on_progress) {
    if (m_busy.exchange(true)) {
        return PackageResult::fail(app_id, "Another operation is already in progress.");
    }
    m_cancel_requested.store(false);

    struct BusyGuard {
        std::atomic<bool>& flag;
        ~BusyGuard() { flag.store(false); }
    } guard{m_busy};

    auto record = m_db->find(app_id);
    if (!record) {
        return PackageResult::fail(app_id, "App not found in installed database: " + app_id);
    }

    logging::Logger::instance().info(TAG, "Uninstall requested: " + app_id);

    auto uninstall_progress = [&](const PackageProgress& inner) {
        if (on_progress) on_progress(inner);
    };

    bool ok = m_installer.uninstall(app_id, record->install_path, uninstall_progress);
    if (!ok) {
        return PackageResult::fail(app_id, "Failed to remove files for: " + app_id);
    }

    if (!m_db->remove(app_id)) {
        logging::Logger::instance().warning(TAG,
            "Uninstalled files but failed to remove DB record for: " + app_id);
    }

    logging::Logger::instance().info(TAG, "Uninstall complete: " + app_id);
    return PackageResult::ok(app_id);
}

} // namespace calcium::packages
