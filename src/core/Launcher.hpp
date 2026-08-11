#pragma once

#include "../platform/IPlatform.hpp"
#include "../config/InstalledDatabase.hpp"
#include "../logging/Logger.hpp"

#include <string>
#include <memory>

namespace calcium::core {

/// Encapsulates the logic for launching an installed application.
///
/// On desktop this invokes an OS process.
/// On PS4 it calls sceSystemServiceLaunchApp via the platform layer.
/// Callers never touch IPlatform directly for launch operations.
class Launcher {
public:
    Launcher(std::shared_ptr<platform::IPlatform>       platform,
             std::shared_ptr<config::InstalledDatabase> installed_db);

    /// Launch the installed application identified by app_id.
    /// Returns true if the launch request was accepted by the OS.
    bool launch(const std::string& app_id);

private:
    std::shared_ptr<platform::IPlatform>       m_platform;
    std::shared_ptr<config::InstalledDatabase> m_db;
};

} // namespace calcium::core
