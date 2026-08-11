#pragma once

#include "../config/Config.hpp"
#include "../config/InstalledDatabase.hpp"
#include "../platform/IPlatform.hpp"
#include "../filesystem/IFilesystem.hpp"
#include "../networking/IHttpClient.hpp"
#include "../repository/RepositoryManager.hpp"
#include "../packages/PackageManager.hpp"
#include "AppManager.hpp"
#include "Launcher.hpp"

#include <memory>
#include <string>
#include <filesystem>

namespace calcium::ui { class UIManager; }

namespace calcium::core {

/// Top-level application object.
///
/// Owns every major subsystem and wires them together during startup.
/// The main loop calls run() which blocks until the user exits.
class Application {
public:
    Application() = default;
    ~Application() = default;

    // Non-copyable.
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    /// Initialise all subsystems from a config file path.
    /// Passing an empty string uses the platform default config path.
    bool init(const std::string& config_path = "");

    /// Enter the main render/event loop. Blocks until exit.
    void run();

    /// Trigger a clean shutdown from outside the main loop (e.g. signal handler).
    void request_exit();

    // ── Accessors for subsystems (used by UI layer) ────────────────────────
    config::Config&            config()       { return m_config; }
    config::InstalledDatabase& installed_db() { return *m_installed_db; }
    AppManager&                app_manager()  { return *m_app_manager; }
    Launcher&                  launcher()     { return *m_launcher; }
    platform::IPlatform&       platform()     { return *m_platform; }

private:
    bool init_logging();
    bool init_platform();
    bool init_filesystem();
    bool init_config(const std::string& config_path);
    bool init_repositories();
    bool init_packages();
    bool init_ui();

    // Subsystems — order matters for construction/destruction.
    std::unique_ptr<platform::IPlatform>               m_platform;
    std::shared_ptr<filesystem::IFilesystem>            m_fs;
    std::shared_ptr<networking::IHttpClient>            m_http;

    config::Config                                      m_config;
    std::shared_ptr<config::InstalledDatabase>          m_installed_db;

    std::shared_ptr<repository::RepositoryManager>      m_repo_manager;
    std::shared_ptr<packages::PackageManager>           m_pkg_manager;
    std::shared_ptr<AppManager>                         m_app_manager;
    std::shared_ptr<Launcher>                           m_launcher;
    std::shared_ptr<ui::UIManager>                      m_ui;

    std::atomic<bool> m_exit_requested{false};
};

} // namespace calcium::core
