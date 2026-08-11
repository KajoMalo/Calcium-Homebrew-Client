#include "SettingsScreen.hpp"
#include <algorithm>
#include <cstdlib>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

// ─── TextField helpers ────────────────────────────────────────────────────────

void SettingsScreen::TextField::render(Renderer& r, const Theme& t) const {
    // Label above the field.
    r.draw_text(label, x, y - r.text_height(t.font_size_small) - 2,
                t.font_size_small, t.text_secondary);
    Color border_c = focused ? t.border_focus : t.border;
    r.fill_rounded_rect(x, y, w, h, t.corner_radius, t.bg_card);
    r.draw_rounded_rect(x, y, w, h, t.corner_radius, border_c);
    std::string display = value.empty() ? placeholder : value;
    Color text_c = value.empty() ? t.text_disabled : t.text_primary;
    r.draw_text_clipped(display, x + t.spacing.sm, y + (h - r.text_height(t.font_size_body)) / 2,
                        w - t.spacing.md, t.font_size_body, text_c);
    // Cursor blink when focused.
    if (focused) {
        int cx = x + t.spacing.sm + r.text_width(value, t.font_size_body) + 1;
        Uint32 ticks = SDL_GetTicks();
        if ((ticks / 530) % 2 == 0) {
            r.fill_rect(cx, y + 6, 2, h - 12, t.text_primary);
        }
    }
}

bool SettingsScreen::TextField::handle_click(int mx, int my) {
    focused = (mx >= x && mx < x + w && my >= y && my < y + h);
    return focused;
}

bool SettingsScreen::TextField::handle_key(int sym) {
    if (!focused) return false;
#ifdef CALCIUM_DESKTOP_SDL
    if (sym == SDLK_BACKSPACE && !value.empty()) {
        value.pop_back();
        return true;
    }
    if (sym == SDLK_ESCAPE || sym == SDLK_RETURN) {
        focused = false;
        return true;
    }
#endif
    return false;
}

bool SettingsScreen::TextField::handle_text(const char* text) {
    if (!focused) return false;
    value += text;
    return true;
}

// ─── Toggle helpers ───────────────────────────────────────────────────────────

void SettingsScreen::Toggle::render(Renderer& r, const Theme& t) const {
    r.draw_text(label, x, y + (h - r.text_height(t.font_size_body)) / 2,
                t.font_size_body, t.text_primary);
    // Track.
    int track_x = x + w - 50, track_y = y + (h - 22) / 2;
    Color track_c = value ? t.accent : t.bg_card;
    r.fill_rounded_rect(track_x, track_y, 44, 22, 11, track_c);
    r.draw_rounded_rect(track_x, track_y, 44, 22, 11, t.border);
    // Thumb.
    int thumb_x = value ? track_x + 22 : track_x + 2;
    r.fill_circle(thumb_x + 9, track_y + 11, 9, t.text_primary);
}

bool SettingsScreen::Toggle::handle_click(int mx, int my) {
    if (mx >= x && mx < x + w && my >= y && my < y + h) {
        value = !value;
        return true;
    }
    return false;
}

// ─── Dropdown helpers ─────────────────────────────────────────────────────────

void SettingsScreen::Dropdown::render(Renderer& r, const Theme& t) const {
    r.draw_text(label, x, y - r.text_height(t.font_size_small) - 2,
                t.font_size_small, t.text_secondary);
    r.fill_rounded_rect(x, y, w, h, t.corner_radius, t.bg_card);
    r.draw_rounded_rect(x, y, w, h, t.corner_radius, open ? t.border_focus : t.border);
    if (!options.empty()) {
        r.draw_text_clipped(options[selected_idx],
                            x + t.spacing.sm, y + (h - r.text_height(t.font_size_body)) / 2,
                            w - 30, t.font_size_body, t.text_primary);
    }
    // Arrow.
    r.draw_text(open ? "▲" : "▼", x + w - 22,
                y + (h - r.text_height(t.font_size_small)) / 2,
                t.font_size_small, t.text_secondary);

    if (open) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            int oy = y + h + i * h;
            Color bg = (i == selected_idx) ? t.bg_selected : t.bg_card;
            r.fill_rounded_rect(x, oy, w, h, t.corner_radius, bg);
            r.draw_rounded_rect(x, oy, w, h, t.corner_radius, t.border);
            r.draw_text_clipped(options[i], x + t.spacing.sm,
                                oy + (h - r.text_height(t.font_size_body)) / 2,
                                w - t.spacing.md, t.font_size_body, t.text_primary);
        }
    }
}

bool SettingsScreen::Dropdown::handle_click(int mx, int my) {
    if (mx >= x && mx < x + w && my >= y && my < y + h) {
        open = !open;
        return true;
    }
    if (open) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            int oy = y + h + i * h;
            if (mx >= x && mx < x + w && my >= oy && my < oy + h) {
                selected_idx = i;
                open = false;
                return true;
            }
        }
        open = false;
    }
    return false;
}

// ─── SettingsScreen ───────────────────────────────────────────────────────────

SettingsScreen::SettingsScreen(config::Config& config, SaveCallback save_cb)
    : m_config(config)
    , m_save_cb(std::move(save_cb))
    , m_field_download_dir{"Download directory", {}, "./downloads", false, 0,0,0,0}
    , m_field_install_dir {"Install directory",  {}, "./installed",  false, 0,0,0,0}
    , m_field_log_file    {"Log file path",       {}, "./calcium.log",false, 0,0,0,0}
    , m_drop_log_level    {"Log level", {"debug","info","warning","error"}, 1, false, 0,0,0,0}
    , m_toggle_verify     {"Verify package hashes", true, 0,0,0,0}
    , m_field_timeout     {"Download timeout (seconds)", {}, "30",   false, 0,0,0,0}
    , m_field_retries     {"Max retries", {}, "3",                   false, 0,0,0,0}
    , m_field_new_repo_name{"Repository name", {}, "My Repo",        false, 0,0,0,0}
    , m_field_new_repo_url {"Repository URL",  {}, "https://...",    false, 0,0,0,0}
    , m_btn_add_repo("Add Repository", 0, 0, 160, 36)
    , m_btn_save("Save Settings", 0, 0, 160, 40)
{
    m_btn_save.on_click([this]() {
        apply_to_config();
        if (m_save_cb) m_save_cb(m_config.data());
        m_config.save();
        m_status_message = "Settings saved.";
    });

    m_btn_add_repo.on_click([this]() {
        if (m_field_new_repo_url.value.empty()) {
            m_status_message = "Repository URL cannot be empty.";
            return;
        }
        config::RepositorySource src;
        src.id      = "repo_" + std::to_string(std::rand() % 99999);
        src.name    = m_field_new_repo_name.value.empty()
                      ? m_field_new_repo_url.value
                      : m_field_new_repo_name.value;
        src.url     = m_field_new_repo_url.value;
        src.enabled = true;
        m_config.add_repository(src);
        m_repo_rows.emplace_back(src);
        m_field_new_repo_name.value.clear();
        m_field_new_repo_url.value.clear();
        m_status_message = "Repository added. Save settings to persist.";
    });
}

void SettingsScreen::build_fields() {
    const auto& d = m_config.data();
    m_field_download_dir.value = d.download_dir;
    m_field_install_dir.value  = d.install_dir;
    m_field_log_file.value     = d.log_file;
    m_toggle_verify.value      = d.verify_hashes;
    m_field_timeout.value      = std::to_string(d.download_timeout_secs);
    m_field_retries.value      = std::to_string(d.download_max_retries);

    // Set dropdown to current log level.
    const std::vector<std::string> levels = {"debug","info","warning","error"};
    for (int i = 0; i < static_cast<int>(levels.size()); ++i) {
        if (levels[i] == d.log_level) { m_drop_log_level.selected_idx = i; break; }
    }

    // Rebuild repo rows.
    m_repo_rows.clear();
    for (const auto& src : d.repositories) {
        m_repo_rows.emplace_back(src);
    }
}

void SettingsScreen::apply_to_config() {
    auto& d = m_config.data();
    if (!m_field_download_dir.value.empty()) d.download_dir         = m_field_download_dir.value;
    if (!m_field_install_dir.value.empty())  d.install_dir          = m_field_install_dir.value;
    if (!m_field_log_file.value.empty())     d.log_file             = m_field_log_file.value;
    d.log_level             = m_drop_log_level.value();
    d.verify_hashes         = m_toggle_verify.value;
    if (!m_field_timeout.value.empty()) {
        try { d.download_timeout_secs = std::stoi(m_field_timeout.value); } catch (...) {}
    }
    if (!m_field_retries.value.empty()) {
        try { d.download_max_retries = std::stoi(m_field_retries.value); } catch (...) {}
    }

    // Sync repo enabled states from rows.
    for (const auto& row : m_repo_rows) {
        for (auto& src : d.repositories) {
            if (src.id == row.id) { src.enabled = row.enabled; break; }
        }
    }
}

void SettingsScreen::focused_blur_all() {
    m_field_download_dir.focused = false;
    m_field_install_dir.focused  = false;
    m_field_log_file.focused     = false;
    m_field_timeout.focused      = false;
    m_field_retries.focused      = false;
    m_field_new_repo_name.focused = false;
    m_field_new_repo_url.focused  = false;
}

void SettingsScreen::on_enter() {
    build_fields();
    m_scroll_y = 0;
    m_status_message.clear();
}

void SettingsScreen::update(float /*dt*/) {
    // Layout all fields relative to current window size.
    const int lx = 40, fw = std::min(500, m_w - 80), fh = 38, gap = 54;
    int y = 80 - m_scroll_y;
    auto place = [&](auto& f) { f.x = lx; f.y = y; f.w = fw; f.h = fh; y += gap; };

    place(m_field_download_dir);
    place(m_field_install_dir);
    place(m_field_log_file);

    // Dropdown is a bit smaller.
    m_drop_log_level.x = lx; m_drop_log_level.y = y; m_drop_log_level.w = 200; m_drop_log_level.h = fh;
    y += gap;

    m_toggle_verify.x = lx; m_toggle_verify.y = y; m_toggle_verify.w = fw; m_toggle_verify.h = fh;
    y += gap;

    place(m_field_timeout);
    place(m_field_retries);

    y += 16;
    // Repo section.
    int row_h = 44;
    for (auto& row : m_repo_rows) {
        row.btn_toggle.set_position(m_w - 200, y + (row_h - 28) / 2);
        row.btn_toggle.set_label(row.enabled ? "Disable" : "Enable");
        row.btn_remove.set_position(m_w - 110, y + (row_h - 28) / 2);
        y += row_h;
    }

    y += 8;
    m_field_new_repo_name.x = lx; m_field_new_repo_name.y = y;
    m_field_new_repo_name.w = 220; m_field_new_repo_name.h = fh;
    m_field_new_repo_url.x  = lx + 236; m_field_new_repo_url.y = y;
    m_field_new_repo_url.w  = fw - 236; m_field_new_repo_url.h = fh;
    m_btn_add_repo.set_position(lx + fw + 12, y);
    y += gap;

    m_btn_save.set_position(lx, y);
}

void SettingsScreen::render(Renderer& r, const Theme& t) {
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text("Settings", t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_line(0, 64, m_w, 64, t.border);

    r.set_clip(0, 64, m_w, m_h - 64);

    // Section: Paths.
    int sy = 80 - m_scroll_y;
    r.draw_text("Paths", 40, sy - 24, t.font_size_sub, t.text_primary);
    m_field_download_dir.render(r, t);
    m_field_install_dir.render(r, t);
    m_field_log_file.render(r, t);

    // Section: Logging.
    r.draw_text("Logging", 40, m_drop_log_level.y - 40, t.font_size_sub, t.text_primary);
    m_drop_log_level.render(r, t);

    // Section: Packages.
    r.draw_text("Packages", 40, m_toggle_verify.y - 30, t.font_size_sub, t.text_primary);
    m_toggle_verify.render(r, t);
    m_field_timeout.render(r, t);
    m_field_retries.render(r, t);

    // Section: Repositories.
    int repo_header_y = m_field_retries.y + 54 + 12;
    r.draw_text("Repositories", 40, repo_header_y, t.font_size_sub, t.text_primary);
    repo_header_y += 28;

    for (auto& row : m_repo_rows) {
        int ry = repo_header_y;
        r.fill_rect(0, ry, m_w, 44, t.bg_secondary);
        r.draw_line(0, ry + 43, m_w, ry + 43, t.border);

        Color name_c = row.enabled ? t.text_primary : t.text_disabled;
        r.draw_text_clipped(row.name, 40, ry + 8, 300, t.font_size_body, name_c);
        r.draw_text_clipped(row.url,  40, ry + 24, 300, t.font_size_small, t.text_secondary);

        row.btn_toggle.render(r, t);
        row.btn_remove.render(r, t);
        repo_header_y += 44;
    }

    if (m_repo_rows.empty()) {
        r.draw_text("No repositories configured.", 40, repo_header_y,
                    t.font_size_body, t.text_secondary);
        repo_header_y += 28;
    }

    // Add repo row.
    repo_header_y += 8;
    r.draw_text("Add repository:", 40, repo_header_y, t.font_size_small, t.text_secondary);
    repo_header_y += 20;
    m_field_new_repo_name.y = repo_header_y;
    m_field_new_repo_url.y  = repo_header_y;
    m_btn_add_repo.set_position(m_btn_add_repo.x(), repo_header_y + 4);
    m_field_new_repo_name.render(r, t);
    m_field_new_repo_url.render(r, t);
    m_btn_add_repo.render(r, t);

    // Save button.
    int save_y = repo_header_y + 60;
    m_btn_save.set_position(40, save_y);
    m_btn_save.render(r, t);

    if (!m_status_message.empty()) {
        r.draw_text(m_status_message, 40, save_y + 48,
                    t.font_size_small, t.success);
    }

    r.clear_clip();
}

#ifdef CALCIUM_DESKTOP_SDL
bool SettingsScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1; m_h = ev.window.data2; return false;
    }
    if (ev.type == SDL_MOUSEWHEEL) {
        m_scroll_y = std::max(0, m_scroll_y - ev.wheel.y * 30);
        return true;
    }
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int mx = ev.button.x, my = ev.button.y;
        focused_blur_all();
        if (m_field_download_dir.handle_click(mx, my)) return true;
        if (m_field_install_dir.handle_click(mx, my))  return true;
        if (m_field_log_file.handle_click(mx, my))     return true;
        if (m_field_timeout.handle_click(mx, my))      return true;
        if (m_field_retries.handle_click(mx, my))      return true;
        if (m_drop_log_level.handle_click(mx, my))     return true;
        if (m_toggle_verify.handle_click(mx, my))      return true;
        if (m_field_new_repo_name.handle_click(mx, my)) return true;
        if (m_field_new_repo_url.handle_click(mx, my))  return true;
        if (m_btn_add_repo.handle_mouse_click(mx, my))  return true;
        if (m_btn_save.handle_mouse_click(mx, my))      return true;
        for (auto& row : m_repo_rows) {
            if (row.btn_toggle.handle_mouse_click(mx, my)) {
                row.enabled = !row.enabled;
                return true;
            }
            if (row.btn_remove.handle_mouse_click(mx, my)) {
                m_config.remove_repository(row.id);
                m_repo_rows.erase(
                    std::remove_if(m_repo_rows.begin(), m_repo_rows.end(),
                        [&](const RepoRow& r) { return r.id == row.id; }),
                    m_repo_rows.end());
                m_status_message = "Repository removed. Save to persist.";
                return true;
            }
        }
    }
    if (ev.type == SDL_MOUSEMOTION) {
        m_btn_save.handle_mouse_move(ev.motion.x, ev.motion.y);
        m_btn_add_repo.handle_mouse_move(ev.motion.x, ev.motion.y);
        for (auto& row : m_repo_rows) {
            row.btn_toggle.handle_mouse_move(ev.motion.x, ev.motion.y);
            row.btn_remove.handle_mouse_move(ev.motion.x, ev.motion.y);
        }
    }
    if (ev.type == SDL_KEYDOWN) {
        auto sym = ev.key.keysym.sym;
        if (m_field_download_dir.handle_key(sym)) return true;
        if (m_field_install_dir.handle_key(sym))  return true;
        if (m_field_log_file.handle_key(sym))     return true;
        if (m_field_timeout.handle_key(sym))      return true;
        if (m_field_retries.handle_key(sym))      return true;
        if (m_field_new_repo_name.handle_key(sym)) return true;
        if (m_field_new_repo_url.handle_key(sym))  return true;
    }
    if (ev.type == SDL_TEXTINPUT) {
        if (m_field_download_dir.handle_text(ev.text.text)) return true;
        if (m_field_install_dir.handle_text(ev.text.text))  return true;
        if (m_field_log_file.handle_text(ev.text.text))     return true;
        if (m_field_timeout.handle_text(ev.text.text))      return true;
        if (m_field_retries.handle_text(ev.text.text))      return true;
        if (m_field_new_repo_name.handle_text(ev.text.text)) return true;
        if (m_field_new_repo_url.handle_text(ev.text.text))  return true;
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
