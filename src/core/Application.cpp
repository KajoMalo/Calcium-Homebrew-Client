#include "Application.hpp"
#include "../ui/UIManager.hpp"
#include "../logging/Logger.hpp"
#include "../logging/FileSink.hpp"

#include <iostream>
#include <thread>

namespace calcium::core {

static constexpr std::string_view TAG = "Application";

// ─── Public API ───────────────────────────────────────────────────────────────

bool Application::init(const std::string& config_path) {
    // Logging first — everything else can log.
    if (!init_logging()) return false;

    logging::Logger::instance().info(TAG,
        "Calcium Client v1.0.0 starting up...");

    if (!init_platform())              return false;
    if (!init_filesystem())            return false;
    if (!init_config(config_path))     return false;
    if (!init_repositories())          return false;
    if (!init_packages())              return false;
    if (!init_ui())                    return false;

    logging::Logger::instance().info(TAG, "All subsystems initialised.");
    return true;
}

void Application::run() {
    logging::Logger::instance().info(TAG, "Entering main loop.");

    // Kick off an async catalog refresh so the UI has data quickly.
    std::thread refresh_thread([this]() {
        m_app_manager->refresh_catalog();
    });
    refresh_thread.detach();

    // Main loop — delegate to UIManager which owns screen management.
    while (!m_exit_requested.load()) {
        if (!m_platform->poll_events()) break;
        m_ui->update();
        m_ui->render();
        m_platform->present_frame();
    }

    logging::Logger::instance().info(TAG, "Main loop exited. Shutting down.");
    m_platform->shutdown();
}

void Application::request_exit() {
    m_exit_requested.store(true);
}

// ─── Initialisation stages ────────────────────────────────────────────────────

bool Application::init_logging() {
    // Console sink is created automatically by Logger's constructor.
    // File sink is added after config is loaded (we need the log file path).
    logging::Logger::instance().set_level(logging::LogLevel::Info);
    return true;
}

bool Application::init_platform() {
    m_platform = platform::IPlatform::create();
    if (!m_platform->init()) {
        std::cerr << "[Application] Platform init failed.\n";
        return false;
    }
    return true;
}

bool Application::init_filesystem() {
    m_fs   = m_platform->create_filesystem();
    m_http = m_platform->create_http_client();
    return true;
}

bool Application::init_config(const std::string& override_path) {
    std::string path = override_path.empty()
        ? m_platform->config_file_path()
        : override_path;

    logging::Logger::instance().info(TAG, "Loading config from: " + path);

    if (!m_config.load(path)) {
        logging::Logger::instance().warning(TAG, "Config load failed; using defaults.");
    }

    // Apply log level from config.
    auto level = logging::parse_log_level(m_config.log_level());
    logging::Logger::instance().set_level(level);

    // Add file sink now that we know the log file path.
    if (!m_config.data().log_file.empty()) {
        auto file_sink = std::make_shared<logging::FileSink>(m_config.data().log_file);
        if (file_sink->is_open()) {
            logging::Logger::instance().add_sink(file_sink);
            logging::Logger::instance().info(TAG,
                "Logging to file: " + m_config.data().log_file);
        }
    }

    // Ensure download and install directories exist.
    m_fs->create_directories(m_config.data().download_dir);
    m_fs->create_directories(m_config.data().install_dir);

    // Load installed database.
    m_installed_db = std::make_shared<config::InstalledDatabase>();
    auto db_path = std::filesystem::path(m_platform->app_data_path()) / "installed.json";
    m_installed_db->load(db_path);

    return true;
}

bool Application::init_repositories() {
    m_repo_manager = std::make_shared<repository::RepositoryManager>(m_http);

    // If config has no repos, seed with the default mock repo for development.
    auto& repos = m_config.data().repositories;
    if (repos.empty()) {
        logging::Logger::instance().warning(TAG,
            "No repositories configured. Add one in Settings or config.json.");
    } else {
        m_repo_manager->configure(repos);
    }

    return true;
}

bool Application::init_packages() {
    m_pkg_manager = std::make_shared<packages::PackageManager>(
        m_http, m_fs, m_installed_db, m_config.data());

    m_app_manager = std::make_shared<AppManager>(
        m_repo_manager, m_installed_db, m_pkg_manager);

    m_launcher = std::make_shared<Launcher>(
        std::shared_ptr<platform::IPlatform>(m_platform.get(), [](auto*){}),
        m_installed_db);

    return true;
}

bool Application::init_ui() {
    m_ui = std::make_shared<ui::UIManager>(
        *this,
        m_platform->display_info().width,
        m_platform->display_info().height
    );
    return m_ui->init();
}

} // namespace calcium::core
