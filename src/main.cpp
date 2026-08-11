#include "core/Application.hpp"
#include "logging/Logger.hpp"

#include <iostream>
#include <string>
#include <csignal>

// Global pointer for signal handler — only used for clean shutdown.
static calcium::core::Application* g_app = nullptr;

static void signal_handler(int /*sig*/) {
    if (g_app) g_app->request_exit();
}

int main(int argc, char* argv[]) {
    // ── Parse optional config path argument ───────────────────────────────
    std::string config_path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Calcium Client v1.0.0\n"
                << "Usage: calcium-client [--config <path>]\n"
                << "  --config, -c   Path to config JSON file.\n"
                << "                 Defaults to the platform-specific user data directory.\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "Calcium Client 1.0.0\n";
            return 0;
        }
    }

    // ── Install signal handlers for graceful shutdown ──────────────────────
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // ── Initialise and run ────────────────────────────────────────────────
    calcium::core::Application app;
    g_app = &app;

    if (!app.init(config_path)) {
        std::cerr << "[calcium-client] Initialisation failed. Check the log for details.\n";
        return 1;
    }

    app.run();

    g_app = nullptr;
    return 0;
}
