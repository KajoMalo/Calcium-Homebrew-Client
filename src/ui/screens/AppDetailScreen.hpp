#pragma once

#include "IScreen.hpp"
#include "../../core/AppManager.hpp"
#include "../../core/Launcher.hpp"
#include "../widgets/Button.hpp"
#include "../widgets/ProgressBar.hpp"
#include <string>
#include <functional>
#include <optional>

namespace calcium::ui::screens {

class AppDetailScreen final : public IScreen {
public:
    using BackCallback = std::function<void()>;

    AppDetailScreen(core::AppManager& app_manager,
                    core::Launcher& launcher,
                    BackCallback back_cb);

    void set_app_id(const std::string& app_id);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    void refresh_state();
    void on_app_event(const core::AppEvent& ev);

    core::AppManager&   m_app_manager;
    core::Launcher&     m_launcher;
    BackCallback        m_back;

    std::string         m_app_id;
    std::optional<repository::AppMetadata> m_meta;

    // Action buttons.
    widgets::Button     m_btn_primary;   // Install / Launch / Update
    widgets::Button     m_btn_secondary; // Uninstall / Back
    widgets::ProgressBar m_progress;

    bool  m_op_active   = false;
    float m_op_progress = 0.0f;
    std::string m_op_message;
    std::string m_error_message;

    int m_scroll_y = 0;
    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
