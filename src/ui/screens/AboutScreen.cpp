#include "AboutScreen.hpp"
#include <string>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

AboutScreen::AboutScreen(platform::IPlatform& platform)
    : m_platform(platform)
{}

void AboutScreen::on_enter() {
    m_sys_info = m_platform.system_info();
}

void AboutScreen::render(Renderer& r, const Theme& t) {
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text("About", t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_line(0, 64, m_w, 64, t.border);

    // ── Logo / title block ────────────────────────────────────────────────
    int cx = m_w / 2;
    int y  = 100;

    // Large "Ca" logo.
    r.fill_rounded_rect(cx - 48, y, 96, 96, 18, t.bg_card);
    r.draw_rounded_rect(cx - 48, y, 96, 96, 18, t.border);
    r.draw_text_centered("Ca", cx - 48, y + (96 - r.text_height(t.font_size_hero)) / 2,
                         96, t.font_size_hero, t.accent);
    y += 96 + t.spacing.md;

    r.draw_text_centered("Calcium Client", 0, y, m_w, t.font_size_hero, t.text_primary);
    y += r.text_height(t.font_size_hero) + t.spacing.xs;

    r.draw_text_centered("Version 1.0.0", 0, y, m_w, t.font_size_sub, t.text_secondary);
    y += r.text_height(t.font_size_sub) + t.spacing.sm;

    r.draw_text_centered("Homebrew Software Distribution Client for PS4",
                         0, y, m_w, t.font_size_body, t.text_secondary);
    y += r.text_height(t.font_size_body) + t.spacing.xl;

    // Divider.
    r.draw_line(cx - 200, y, cx + 200, y, t.border);
    y += t.spacing.lg;

    // ── System info ───────────────────────────────────────────────────────
    r.draw_text_centered("System Information", 0, y, m_w, t.font_size_sub, t.text_primary);
    y += r.text_height(t.font_size_sub) + t.spacing.md;

    auto info_row = [&](const std::string& label, const std::string& value) {
        int lw = 200;
        int vx = cx - lw / 2 + lw + 16;
        r.draw_text(label, cx - lw / 2, y, t.font_size_body, t.text_secondary);
        r.draw_text_clipped(value, vx, y, m_w / 2 - lw, t.font_size_body, t.text_primary);
        y += r.text_height(t.font_size_body) + t.spacing.xs;
    };

    info_row("Platform:",   m_sys_info.platform_name);
    info_row("OS Version:", m_sys_info.os_version);
    info_row("CPU:",        m_sys_info.cpu_model);

    if (m_sys_info.total_ram_bytes > 0) {
        std::string ram = std::to_string(m_sys_info.total_ram_bytes / (1024*1024*1024)) + " GB";
        info_row("Total RAM:", ram);
    }
    if (m_sys_info.free_ram_bytes > 0) {
        std::string free_ram = std::to_string(m_sys_info.free_ram_bytes / (1024*1024)) + " MB";
        info_row("Free RAM:", free_ram);
    }

    y += t.spacing.lg;
    r.draw_line(cx - 200, y, cx + 200, y, t.border);
    y += t.spacing.md;

    // ── Credits / licence ─────────────────────────────────────────────────
    r.draw_text_centered("Open-source software released under the MIT License.",
                         0, y, m_w, t.font_size_small, t.text_secondary);
    y += r.text_height(t.font_size_small) + t.spacing.xs;
    r.draw_text_centered("Built with: C++17, CMake, SDL2, SDL_ttf, nlohmann/json, zlib",
                         0, y, m_w, t.font_size_small, t.text_secondary);
    y += r.text_height(t.font_size_small) + t.spacing.xs;
    r.draw_text_centered("SHA-256 implementation is public domain.",
                         0, y, m_w, t.font_size_small, t.text_secondary);
}

#ifdef CALCIUM_DESKTOP_SDL
bool AboutScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
