#pragma once

#include "../repository/AppMetadata.hpp"
#include "../repository/RepositoryManager.hpp"
#include "../config/InstalledDatabase.hpp"
#include "../packages/IPackageManager.hpp"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <mutex>
#include <thread>
#include <atomic>

namespace calcium::core {

/// High-level install/uninstall event delivered to the UI.
struct AppEvent {
    enum class Type { InstallStarted, InstallProgress, InstallComplete,
                      InstallFailed,  UninstallStarted, UninstallComplete,
                      UninstallFailed, CatalogRefreshed };
    Type        type;
    std::string app_id;
    float       progress      = 0.0f;
    std::string message;
    bool        success       = false;
};

using AppEventCallback = std::function<void(const AppEvent&)>;

/// Coordinates repository browsing, install state, and package operations.
///
/// AppManager is the single source of truth that the UI queries for:
///   - The full catalogue (repository apps with install-state overlaid)
///   - Which apps are installed and at which version
///   - Triggering install / uninstall operations on background threads
class AppManager {
public:
    AppManager(
        std::shared_ptr<repository::RepositoryManager> repo_manager,
        std::shared_ptr<config::InstalledDatabase>     installed_db,
        std::shared_ptr<packages::IPackageManager>     pkg_manager
    );

    ~AppManager();

    // ── Event subscription ─────────────────────────────────────────────────
    void set_event_callback(AppEventCallback cb);

    // ── Catalogue ──────────────────────────────────────────────────────────

    /// Refresh all repositories (blocking — call from background thread).
    void refresh_catalog();

    /// Flat list of all known apps, install-state overlaid.
    std::vector<repository::AppMetadata> catalog() const;

    /// Apps filtered by category.
    std::vector<repository::AppMetadata> catalog_by_category(const std::string& cat) const;

    /// Full-text search across name / description / author / tags.
    std::vector<repository::AppMetadata> search(const std::string& query) const;

    /// Find a single app by id.
    std::optional<repository::AppMetadata> find_app(const std::string& app_id) const;

    /// Apps that are installed and have a newer version available in the repo.
    std::vector<repository::AppMetadata> available_updates() const;

    // ── Install / uninstall (non-blocking, run on internal thread) ─────────

    /// Queue an install. Returns false immediately if already busy with this id.
    bool install(const std::string& app_id);

    /// Queue an uninstall. Returns false if not installed.
    bool uninstall(const std::string& app_id);

    /// Cancel the active install/uninstall.
    void cancel();

    bool is_busy() const;

    // ── Installed queries (cheap, no lock) ─────────────────────────────────
    bool        is_installed(const std::string& app_id) const;
    std::string installed_version(const std::string& app_id) const;

private:
    void overlay_install_state(repository::AppMetadata& meta) const;
    void run_install(repository::AppMetadata meta);
    void run_uninstall(std::string app_id);
    void emit(AppEvent ev);

    std::shared_ptr<repository::RepositoryManager> m_repos;
    std::shared_ptr<config::InstalledDatabase>     m_db;
    std::shared_ptr<packages::IPackageManager>     m_pkg;

    mutable std::mutex  m_callback_mutex;
    AppEventCallback    m_event_cb;

    std::atomic<bool>   m_busy{false};
    std::thread         m_op_thread;  ///< Background operation thread.
};

} // namespace calcium::core
