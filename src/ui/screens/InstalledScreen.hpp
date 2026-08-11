#pragma once

#include "IScreen.hpp"
#include "../../core/AppManager.hpp"
#include "../../core/Launcher.hpp"
#include "../widgets/ListView.hpp"
#include "../widgets/Button.hpp"
#include <functional>

namespace calcium::ui::screens {

class InstalledScreen final : public IScreen {
public:
    using AppCallback = std::function<void(const std::string& app_id)>;

    InstalledScreen(core::AppManager& app_manager,
                    core::Launcher& launcher,
                    AppCallback open_details_cb);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    void rebuild_list();
    void launch_selected();
    void uninstall_selected();

    core::AppManager& m_app_manager;
    core::Launcher&   m_launcher;
    AppCallback       m_open_details;

    widgets::ListView m_list;
    widgets::Button   m_btn_launch;
    widgets::Button   m_btn_uninstall;
    widgets::Button   m_btn_details;

    std::string m_status_message;
    std::string m_error_message;

    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
