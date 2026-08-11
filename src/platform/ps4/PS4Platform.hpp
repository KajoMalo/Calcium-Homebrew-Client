#pragma once

#include "../IPlatform.hpp"

namespace calcium::platform {

/// PS4 (Orbis OS) platform implementation.
///
/// Requires the PlayStation 4 SDK (ps4sdk / OpenOrbis or FOSS SDK).
/// Compilation is gated on CALCIUM_PLATFORM_PS4.
///
/// HTTP: uses libSceHttp (sceHttp*) from libSceNet.
/// Filesystem: wraps sceKernelOpen / sceKernelReadFile via the POSIX-compat layer.
/// Launch: sceAppInstUtil + sceSystemServiceLaunchApp.
/// Display: libSceVideoOut + libSceGnmDriver for the render surface.
class PS4Platform final : public IPlatform {
public:
    PS4Platform() = default;
    ~PS4Platform() override;

    bool init()     override;
    void shutdown() override;

    std::shared_ptr<filesystem::IFilesystem> create_filesystem()  override;
    std::shared_ptr<networking::IHttpClient>  create_http_client() override;

    SystemInfo  system_info()  const override;
    DisplayInfo display_info() const override;

    std::string app_data_path()    const override;
    std::string config_file_path() const override;

    bool launch_app(const std::string& install_path,
                    const std::string& content_id) override;

    bool poll_events()   override;
    void present_frame() override;

private:
    bool m_initialised = false;
};

std::unique_ptr<IPlatform> create_ps4_platform();

} // namespace calcium::platform
