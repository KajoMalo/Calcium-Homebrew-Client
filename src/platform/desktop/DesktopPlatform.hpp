#pragma once

#include "../IPlatform.hpp"

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

#include <string>

namespace calcium::platform {

/// Desktop platform implementation.
///
/// Provides real filesystem and HTTP client implementations.
/// Uses SDL2 for window management and input when CALCIUM_DESKTOP_SDL is
/// defined; falls back to a headless console mode otherwise (useful for
/// running tests in CI without a display).
class DesktopPlatform final : public IPlatform {
public:
    DesktopPlatform() = default;
    ~DesktopPlatform() override;

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

    bool poll_events()  override;
    void present_frame() override;

private:
#ifdef CALCIUM_DESKTOP_SDL
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
#endif
    bool m_initialised = false;
    int  m_display_width  = 1280;
    int  m_display_height = 720;
};

} // namespace calcium::platform
