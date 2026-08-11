#pragma once

#include "IScreen.hpp"
#include "../../platform/IPlatform.hpp"
#include <memory>

namespace calcium::ui::screens {

class AboutScreen final : public IScreen {
public:
    explicit AboutScreen(platform::IPlatform& platform);

    void on_enter() override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    platform::IPlatform& m_platform;
    platform::SystemInfo m_sys_info;
    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
