#include "AppManager.hpp"
#include "../logging/Logger.hpp"

namespace calcium::core {

static constexpr std::string_view TAG = "AppManager";

// ─── Construction ─────────────────────────────────────────────────────────────

AppManager::AppManager(
    std::shared_ptr<repository::RepositoryManager> repo_manager,
    std::shared_ptr<config::InstalledDatabase>     installed_db,
    std::shared_ptr<packages::IPackageManager>     pkg_manager)
    : m_repos(std::move(repo_manager))
    , m_db(std::move(installed_db))
    , m_pkg(std::move(pkg_manager))
{}

AppManager::~AppManager() {
    if (m_op_thread.joinable()) {
        m_pkg->cancel();
        m_op_thread.join();
    }
}

// ─── Event subscription ───────────────────────────────────────────────────────

void AppManager::set_event_callback(AppEventCallback cb) {
    std::lock_guard lock{m_callback_mutex};
    m_event_cb = std::move(cb);
}

void AppManager::emit(AppEvent ev) {
    std::lock_guard lock{m_callback_mutex};
    if (m_event_cb) m_event_cb(ev);
}

// ─── Catalogue ────────────────────────────────────────────────────────────────

void AppManager::refresh_catalog() {
    logging::Logger::instance().info(TAG, "Refreshing catalog...");
    m_repos->refresh_all([this](const std::string& repo_id, bool ok) {
        logging::Logger::instance().log(
            ok ? logging::LogLevel::Info : logging::LogLevel::Warning,
            TAG,
            (ok ? "Repo refreshed: " : "Repo failed: ") + repo_id);
    });

    AppEvent ev;
    ev.type    = AppEvent::Type::CatalogRefreshed;
    ev.success = true;
    ev.message = "Catalog refreshed.";
    emit(ev);
    logging::Logger::instance().info(TAG, "Catalog refresh complete.");
}

void AppManager::overlay_install_state(repository::AppMetadata& meta) const {
    auto rec = m_db->find(meta.id);
    if (rec) {
        meta.is_installed       = true;
        meta.installed_version  = rec->version;
    } else {
        meta.is_installed      = false;
        meta.installed_version = {};
    }
}

std::vector<repository::AppMetadata> AppManager::catalog() const {
    auto apps = m_repos->all_apps();
    for (auto& app : apps) overlay_install_state(app);
    return apps;
}

std::vector<repository::AppMetadata>
AppManager::catalog_by_category(const std::string& cat) const {
    auto apps = m_repos->apps_by_category(cat);
    for (auto& app : apps) overlay_install_state(app);
    return apps;
}

std::vector<repository::AppMetadata>
AppManager::search(const std::string& query) const {
    auto apps = m_repos->search(query);
    for (auto& app : apps) overlay_install_state(app);
    return apps;
}

std::optional<repository::AppMetadata>
AppManager::find_app(const std::string& app_id) const {
    auto meta = m_repos->find_app(app_id);
    if (meta) overlay_install_state(*meta);
    return meta;
}

std::vector<repository::AppMetadata> AppManager::available_updates() const {
    auto apps = catalog();
    apps.erase(
        std::remove_if(apps.begin(), apps.end(),
            [](const repository::AppMetadata& m) { return !m.has_update(); }),
        apps.end()
    );
    return apps;
}

// ─── Install state ────────────────────────────────────────────────────────────

bool AppManager::is_installed(const std::string& app_id) const {
    return m_db->is_installed(app_id);
}

std::string AppManager::installed_version(const std::string& app_id) const {
    auto rec = m_db->find(app_id);
    return rec ? rec->version : std::string{};
}

// ─── Install / uninstall ──────────────────────────────────────────────────────

bool AppManager::is_busy() const { return m_busy.load(); }

bool AppManager::install(const std::string& app_id) {
    if (m_busy.load()) {
        logging::Logger::instance().warning(TAG, "Install requested but manager is busy.");
        return false;
    }

    auto meta = find_app(app_id);
    if (!meta) {
        logging::Logger::instance().error(TAG, "Install: unknown app id: " + app_id);
        return false;
    }

    // Join any previous thread.
    if (m_op_thread.joinable()) m_op_thread.join();

    m_busy.store(true);
    m_op_thread = std::thread([this, m = std::move(*meta)]() mutable {
        run_install(std::move(m));
        m_busy.store(false);
    });

    return true;
}

bool AppManager::uninstall(const std::string& app_id) {
    if (m_busy.load()) {
        logging::Logger::instance().warning(TAG, "Uninstall requested but manager is busy.");
        return false;
    }
    if (!m_db->is_installed(app_id)) {
        logging::Logger::instance().warning(TAG, "Uninstall: not installed: " + app_id);
        return false;
    }

    if (m_op_thread.joinable()) m_op_thread.join();

    m_busy.store(true);
    m_op_thread = std::thread([this, id = app_id]() {
        run_uninstall(id);
        m_busy.store(false);
    });

    return true;
}

void AppManager::cancel() {
    logging::Logger::instance().info(TAG, "Cancel requested.");
    m_pkg->cancel();
}

// ─── Background operations ────────────────────────────────────────────────────

void AppManager::run_install(repository::AppMetadata meta) {
    {
        AppEvent ev;
        ev.type    = AppEvent::Type::InstallStarted;
        ev.app_id  = meta.id;
        ev.message = "Starting install of " + meta.name;
        emit(ev);
    }

    auto progress_cb = [this, &meta](const packages::PackageProgress& pp) {
        AppEvent ev;
        ev.type     = AppEvent::Type::InstallProgress;
        ev.app_id   = meta.id;
        ev.progress = pp.progress;
        ev.message  = pp.status_message;
        emit(ev);
    };

    auto result = m_pkg->install(meta, progress_cb);

    AppEvent ev;
    ev.app_id  = meta.id;
    ev.success = result.success;
    ev.message = result.success
        ? meta.name + " installed successfully."
        : "Install failed: " + result.error;
    ev.type    = result.success
        ? AppEvent::Type::InstallComplete
        : AppEvent::Type::InstallFailed;
    emit(ev);

    logging::Logger::instance().log(
        result.success ? logging::LogLevel::Info : logging::LogLevel::Error,
        TAG, ev.message);
}

void AppManager::run_uninstall(std::string app_id) {
    {
        AppEvent ev;
        ev.type   = AppEvent::Type::UninstallStarted;
        ev.app_id = app_id;
        ev.message = "Uninstalling " + app_id;
        emit(ev);
    }

    auto result = m_pkg->uninstall(app_id);

    AppEvent ev;
    ev.app_id  = app_id;
    ev.success = result.success;
    ev.message = result.success
        ? app_id + " uninstalled."
        : "Uninstall failed: " + result.error;
    ev.type    = result.success
        ? AppEvent::Type::UninstallComplete
        : AppEvent::Type::UninstallFailed;
    emit(ev);
}

} // namespace calcium::core
