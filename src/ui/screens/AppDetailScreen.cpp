#include "AppDetailScreen.hpp"
#include <sstream>

#ifdef CALCIUM_DESKTOP_SDL
#include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

AppDetailScreen::AppDetailScreen(core::AppManager& app_manager,
                                  core::Launcher& launcher,
                                  BackCallback back_cb)
    : m_app_manager(app_manager)
    , m_launcher(launcher)
    , m_back(std::move(back_cb))
    , m_btn_primary("Install", 0, 0, 140, 42)
    , m_btn_secondary("Back", 0, 0, 140, 42, widgets::ButtonStyle::Secondary)
    , m_progress(0, 0, 300, 12)
{}

void AppDetailScreen::set_app_id(const std::string& app_id) {
    m_app_id = app_id;
}

void AppDetailScreen::on_enter() {
    refresh_state();
    m_op_active    = false;
    m_op_progress  = 0.0f;
    m_op_message.clear();
    m_error_message.clear();
    m_scroll_y = 0;

    m_app_manager.set_event_callback([this](const core::AppEvent& ev) {
        if (ev.app_id != m_app_id) return;
        on_app_event(ev);
    });
}

void AppDetailScreen::on_app_event(const core::AppEvent& ev) {
    using T = core::AppEvent::Type;
    switch (ev.type) {
        case T::InstallStarted:
            m_op_active = true; m_op_progress = 0.0f;
            m_op_message = "Starting install...";
            break;
        case T::InstallProgress:
            m_op_active   = true;
            m_op_progress = ev.progress;
            m_op_message  = ev.message;
            break;
        case T::InstallComplete:
            m_op_active   = false;
            m_op_progress = 1.0f;
            m_op_message  = ev.message;
            m_error_message.clear();
            refresh_state();
            break;
        case T::InstallFailed:
            m_op_active     = false;
            m_error_message = ev.message;
            m_op_message.clear();
            refresh_state();
            break;
        case T::UninstallStarted:
            m_op_active = true; m_op_progress = 0.0f;
            m_op_message = "Uninstalling...";
            break;
        case T::UninstallComplete:
            m_op_active   = false;
            m_op_message  = "Uninstalled.";
            m_error_message.clear();
            refresh_state();
            break;
        case T::UninstallFailed:
            m_op_active     = false;
            m_error_message = ev.message;
            refresh_state();
            break;
        default: break;
    }
}

void AppDetailScreen::refresh_state() {
    m_meta = m_app_manager.find_app(m_app_id);
    if (!m_meta) return;

    bool installed = m_meta->is_installed;
    bool has_update = m_meta->has_update();

    // Configure primary button.
    if (installed && !has_update) {
        m_btn_primary.set_label("Launch");
        m_btn_primary.on_click([this]() {
            m_launcher.launch(m_app_id);
        });
    } else if (has_update) {
        m_btn_primary.set_label("Update");
        m_btn_primary.on_click([this]() {
            m_app_manager.install(m_app_id);
        });
    } else {
        m_btn_primary.set_label("Install");
        m_btn_primary.on_click([this]() {
            m_app_manager.install(m_app_id);
        });
    }

    // Configure secondary button.
    if (installed) {
        m_btn_secondary.set_label("Uninstall");
        m_btn_secondary.on_click([this]() {
            m_app_manager.uninstall(m_app_id);
        });
    } else {
        m_btn_secondary.set_label("Back");
        m_btn_secondary.on_click([this]() {
            if (m_back) m_back();
        });
    }

    m_btn_primary.set_enabled(!m_op_active);
    m_btn_secondary.set_enabled(!m_op_active);
}

void AppDetailScreen::update(float /*dt*/) {
    if (!m_meta) return;
    m_progress.set_progress(m_op_progress);
    m_btn_primary.set_enabled(!m_op_active && !m_app_manager.is_busy());
    m_btn_secondary.set_enabled(!m_op_active);

    // Layout — place buttons.
    int btn_y = m_h - 80;
    m_btn_primary.set_position(m_w - 320, btn_y);
    m_btn_secondary.set_position(m_w - 160, btn_y);
    m_progress.set_position(m_w - 460, btn_y + 14);
}

void AppDetailScreen::render(Renderer& r, const Theme& t) {
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);

    // Header bar.
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    std::string title = m_meta ? m_meta->name : "App Details";
    r.draw_text(title, t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_line(0, 64, m_w, 64, t.border);

    if (!m_meta) {
        r.draw_text_centered("App not found.", 0, m_h / 2, m_w,
                             t.font_size_body, t.text_secondary);
        return;
    }

    const auto& meta = *m_meta;
    int content_x = t.spacing.lg;
    int content_y = 80;

    // ── Icon + basic info ─────────────────────────────────────────────────
    int icon = t.icon_size_large;
    r.fill_rounded_rect(content_x, content_y, icon, icon, t.corner_radius * 2, t.bg_card);
    if (!meta.name.empty()) {
        std::string init(1, meta.name[0]);
        r.draw_text_centered(init, content_x,
                             content_y + (icon - r.text_height(t.font_size_hero)) / 2,
                             icon, t.font_size_hero, t.accent);
    }

    int info_x = content_x + icon + t.spacing.lg;
    int info_y = content_y;
    r.draw_text(meta.name, info_x, info_y, t.font_size_hero, t.text_primary);
    info_y += r.text_height(t.font_size_hero) + t.spacing.xs;
    r.draw_text("by " + meta.author, info_x, info_y, t.font_size_sub, t.text_secondary);
    info_y += r.text_height(t.font_size_sub) + t.spacing.xs;
    r.draw_text("v" + meta.version, info_x, info_y, t.font_size_body, t.accent);
    info_y += r.text_height(t.font_size_body) + t.spacing.xs;

    // Category / compat badges.
    if (!meta.category.empty()) {
        int bw = r.text_width(meta.category, t.font_size_small) + t.spacing.sm;
        r.fill_rounded_rect(info_x, info_y, bw, 22, 3, t.bg_card);
        r.draw_rounded_rect(info_x, info_y, bw, 22, 3, t.border);
        r.draw_text_centered(meta.category, info_x, info_y + 4, bw,
                             t.font_size_small, t.text_secondary);
    }

    if (meta.is_installed) {
        int badge_x = info_x + r.text_width(meta.category, t.font_size_small) + 60;
        int bw2 = r.text_width("INSTALLED", t.font_size_small) + t.spacing.sm;
        r.fill_rounded_rect(badge_x, info_y, bw2, 22, 3, t.success);
        r.draw_text_centered("INSTALLED", badge_x, info_y + 4, bw2,
                             t.font_size_small, Color{15,15,20});
    }

    // ── Description ───────────────────────────────────────────────────────
    int desc_y = content_y + icon + t.spacing.lg;
    r.draw_line(content_x, desc_y, m_w - content_x, desc_y, t.border);
    desc_y += t.spacing.sm;

    // Wrap long_description manually at word boundaries.
    std::string desc = meta.long_description.empty()
        ? meta.description
        : meta.long_description;
    int max_line_w = m_w - content_x * 2;
    // Simple word-wrap: split by spaces, accumulate, emit lines.
    std::istringstream stream(desc);
    std::string word, line;
    while (stream >> word) {
        std::string candidate = line.empty() ? word : line + " " + word;
        if (r.text_width(candidate, t.font_size_body) <= max_line_w) {
            line = candidate;
        } else {
            if (!line.empty()) {
                r.draw_text(line, content_x, desc_y - m_scroll_y,
                            t.font_size_body, t.text_primary);
                desc_y += r.text_height(t.font_size_body) + 4;
            }
            line = word;
        }
    }
    if (!line.empty()) {
        r.draw_text(line, content_x, desc_y - m_scroll_y,
                    t.font_size_body, t.text_primary);
        desc_y += r.text_height(t.font_size_body) + t.spacing.lg;
    }

    // ── Metadata table ────────────────────────────────────────────────────
    auto row = [&](const std::string& label, const std::string& value) {
        r.draw_text(label, content_x, desc_y - m_scroll_y,
                    t.font_size_body, t.text_secondary);
        r.draw_text(value, content_x + 180, desc_y - m_scroll_y,
                    t.font_size_body, t.text_primary);
        desc_y += r.text_height(t.font_size_body) + t.spacing.xs;
    };

    if (!meta.min_firmware.empty())
        row("Min firmware:", meta.min_firmware);
    if (meta.download_size > 0)
        row("Download size:", std::to_string(meta.download_size / (1024*1024)) + " MB");
    if (meta.installed_size > 0)
        row("Installed size:", std::to_string(meta.installed_size / (1024*1024)) + " MB");
    if (!meta.updated_at.empty())
        row("Updated:", meta.updated_at.substr(0, 10));
    if (!meta.license.empty())
        row("License:", meta.license);

    // ── Changelog ─────────────────────────────────────────────────────────
    if (!meta.changelog.empty()) {
        desc_y += t.spacing.sm;
        r.draw_text("Changelog", content_x, desc_y - m_scroll_y,
                    t.font_size_sub, t.text_primary);
        desc_y += r.text_height(t.font_size_sub) + t.spacing.xs;
        r.draw_text_clipped(meta.changelog, content_x, desc_y - m_scroll_y,
                            max_line_w, t.font_size_body, t.text_secondary);
    }

    // ── Bottom action bar ─────────────────────────────────────────────────
    int bar_y = m_h - 72;
    r.fill_rect(0, bar_y, m_w, 72, t.bg_secondary);
    r.draw_line(0, bar_y, m_w, 1, t.border);

    m_btn_primary.render(r, t);
    m_btn_secondary.render(r, t);

    if (m_op_active) {
        m_progress.render(r, t);
        r.draw_text(m_op_message,
                    m_w - 460, m_h - 56,
                    t.font_size_small, t.text_secondary);
    }

    if (!m_error_message.empty()) {
        r.draw_text_clipped(m_error_message,
                            t.spacing.lg, m_h - 56,
                            m_w / 2,
                            t.font_size_small, t.error_color);
    }
}

#ifdef CALCIUM_DESKTOP_SDL
bool AppDetailScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
        return false;
    }
    if (ev.type == SDL_MOUSEWHEEL) {
        m_scroll_y = std::max(0, m_scroll_y - ev.wheel.y * 30);
        return true;
    }
    if (ev.type == SDL_MOUSEMOTION) {
        m_btn_primary.handle_mouse_move(ev.motion.x, ev.motion.y);
        m_btn_secondary.handle_mouse_move(ev.motion.x, ev.motion.y);
        return false;
    }
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        if (m_btn_primary.handle_mouse_click(ev.button.x, ev.button.y))   return true;
        if (m_btn_secondary.handle_mouse_click(ev.button.x, ev.button.y)) return true;
    }
    if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_BACKSPACE) {
        if (m_back) m_back();
        return true;
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
