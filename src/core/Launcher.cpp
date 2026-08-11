#include "Launcher.hpp"

namespace calcium::core {

static constexpr std::string_view TAG = "Launcher";

Launcher::Launcher(std::shared_ptr<platform::IPlatform>       platform,
                   std::shared_ptr<config::InstalledDatabase> installed_db)
    : m_platform(std::move(platform))
    , m_db(std::move(installed_db))
{}

bool Launcher::launch(const std::string& app_id) {
    auto record = m_db->find(app_id);
    if (!record) {
        logging::Logger::instance().error(TAG,
            "Cannot launch '" + app_id + "': not found in installed database.");
        return false;
    }

    if (record->install_path.empty()) {
        logging::Logger::instance().error(TAG,
            "Cannot launch '" + app_id + "': install_path is empty.");
        return false;
    }

    logging::Logger::instance().info(TAG,
        "Launching '" + record->name + "' from " + record->install_path);

    bool ok = m_platform->launch_app(record->install_path, record->content_id);
    if (!ok) {
        logging::Logger::instance().error(TAG,
            "Platform rejected launch request for '" + app_id + "'.");
    }
    return ok;
}

} // namespace calcium::core
