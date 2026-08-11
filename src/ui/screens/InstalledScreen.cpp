#include "InstalledScreen.hpp"

#ifdef CALCIUM_DESKTOP_SDL
#include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

InstalledScreen::InstalledScreen(core::AppManager& app_manager,
                                  core::Launcher& launcher,
                                  AppCallback open_details_cb)
    : m_app_manager(app_manager)
    , m_launcher(launcher)
    , m_open_details(std::move(open_details_cb))
    , m_list(24, 80, 900, 560)
    , m_btn_launch("Launch",     940, 100, 150, 40)
    , m_btn_uninstall("Uninstall", 940, 156, 150, 40, widgets::ButtonStyle::Danger)
    , m_btn_details("Details",  940, 212, 150, 40, widgets::ButtonStyle::Secondary)
{
    m_list.on_select([this](const std::string& /*id*/) {
        m_status_message.clear();
        m_error_message.clear();
    });

    m_btn_launch.on_click([this]() { launch_selected(); });
    m_btn_uninstall.on_click([this]() { uninstall_selected(); });
    m_btn_details.on_click([this]() {
        if (!m_list.selected_id().empty() && m_open_details)
            m_open_details(m_list.selected_id());
    });
}

void InstalledScreen::on_enter() {
    rebuild_list();

    m_app_manager.set_event_callback([this](const core::AppEvent& ev) {
        using T = core::AppEvent::Type;
        if (ev.type == T::UninstallComplete || ev.type == T::UninstallFailed) {
            m_status_message = ev.success ? "Uninstalled." : "Uninstall failed: " + ev.message;
            m_error_message  = ev.success ? "" : ev.message;
            rebuild_list();
        }
    });
}

void InstalledScreen::rebuild_list() {
    auto installed = m_app_manager.catalog();
    std::vector<widgets::ListItem> items;
    for (const auto& app : installed) {
        if (!app.is_installed) continue;
        widgets::ListItem item;
        item.id        = app.id;
        item.primary   = app.name;
        item.secondary = "v" + app.installed_version + " • " + app.author;
        if (app.has_update()) item.badge = "UPDATE";
        items.push_back(std::move(item));
    }
    m_list.set_items(std::move(items));
}

void InstalledScreen::launch_selected() {
    const auto& id = m_list.selected_id();
    if (id.empty()) return;
    bool ok = m_launcher.launch(id);
    m_status_message = ok ? "Launching..." : "Launch failed.";
    m_error_message  = ok ? "" : "Could not launch application.";
}

void InstalledScreen::uninstall_selected() {
    const auto& id = m_list.selected_id();
    if (id.empty()) return;
    m_status_message = "Uninstalling...";
    m_error_message.clear();
    m_app_manager.uninstall(id);
}

void InstalledScreen::update(float /*dt*/) {
    bool has_sel = !m_list.selected_id().empty();
    m_btn_launch.set_enabled(has_sel);
    m_btn_uninstall.set_enabled(has_sel && !m_app_manager.is_busy());
    m_btn_details.set_enabled(has_sel);

    // Keep button positions relative to window size.
    m_list.set_size(m_w - 200, m_h - 100);
    m_btn_launch.set_position(m_w - 180, 100);
    m_btn_uninstall.set_position(m_w - 180, 156);
    m_btn_details.set_position(m_w - 180, 212);
}

void InstalledScreen::render(Renderer& r, const Theme& t) {
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text("Installed Apps", t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_text(std::to_string(m_list.item_count()) + " installed",
                m_w - 120, 26, t.font_size_small, t.text_secondary);
    r.draw_line(0, 64, m_w, 64, t.border);

    m_list.render(r, t);

    if (m_list.item_count() == 0) {
        r.draw_text_centered("No apps installed yet. Visit the Catalog to install apps.",
                             0, m_h / 2, m_w, t.font_size_body, t.text_secondary);
    }

    m_btn_launch.render(r, t);
    m_btn_uninstall.render(r, t);
    m_btn_details.render(r, t);

    // Status messages.
    if (!m_status_message.empty()) {
        r.draw_text(m_status_message, t.spacing.lg, m_h - 40,
                    t.font_size_small, t.text_secondary);
    }
    if (!m_error_message.empty()) {
        r.draw_text_clipped(m_error_message, t.spacing.lg, m_h - 40,
                            m_w / 2, t.font_size_small, t.error_color);
    }
}

#ifdef CALCIUM_DESKTOP_SDL
bool InstalledScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
        return false;
    }
    if (ev.type == SDL_MOUSEMOTION) {
        m_list.handle_mouse_move(ev.motion.x, ev.motion.y);
        m_btn_launch.handle_mouse_move(ev.motion.x, ev.motion.y);
        m_btn_uninstall.handle_mouse_move(ev.motion.x, ev.motion.y);
        m_btn_details.handle_mouse_move(ev.motion.x, ev.motion.y);
        return false;
    }
    if (ev.type == SDL_MOUSEWHEEL)
        return m_list.handle_mouse_wheel(ev.wheel.y);
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        if (m_btn_launch.handle_mouse_click(ev.button.x, ev.button.y))    return true;
        if (m_btn_uninstall.handle_mouse_click(ev.button.x, ev.button.y)) return true;
        if (m_btn_details.handle_mouse_click(ev.button.x, ev.button.y))   return true;
        return m_list.handle_mouse_click(ev.button.x, ev.button.y);
    }
    if (ev.type == SDL_KEYDOWN)
        return m_list.handle_key(ev.key.keysym.sym);
    return false;
}
#endif

} // namespace calcium::ui::screens
