#include "HomeScreen.hpp"
#include <algorithm>
#include <cmath>

#ifdef CALCIUM_DESKTOP_SDL
#include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

HomeScreen::HomeScreen(core::AppManager& app_manager,
                       NavCallback nav_cb,
                       AppCallback open_app_cb)
    : m_app_manager(app_manager)
    , m_nav(std::move(nav_cb))
    , m_open_app(std::move(open_app_cb))
{}

void HomeScreen::on_enter() {
    refresh_featured();
}

void HomeScreen::refresh_featured() {
    m_featured.clear();
    auto apps = m_app_manager.catalog();
    // Show up to 6 recently-updated apps on the home screen.
    int count = 0;
    for (const auto& app : apps) {
        if (count++ >= 6) break;
        FeaturedItem fi;
        fi.app_id      = app.id;
        fi.name        = app.name;
        fi.author      = app.author;
        fi.description = app.description;
        fi.category    = app.category;
        fi.installed   = app.is_installed;
        m_featured.push_back(std::move(fi));
    }
}

void HomeScreen::update(float /*dt*/) {
    // Rebuild nav items on every frame with current dimensions so they
    // respond to window resize. In a full implementation this would be
    // layout-cached and only rebuilt on resize events.
    m_nav_items.clear();
    struct NavDef { const char* label; const char* target; };
    static const NavDef defs[] = {
        {"Catalog",   "catalog"},
        {"Installed", "installed"},
        {"Downloads", "downloads"},
        {"Settings",  "settings"},
    };
    int btn_w = 140, btn_h = 40, gap = 12;
    int start_x = (m_w - (4 * btn_w + 3 * gap)) / 2;
    int btn_y   = m_h - 80;
    for (int i = 0; i < 4; ++i) {
        NavItem ni;
        ni.label   = defs[i].label;
        ni.target  = defs[i].target;
        ni.x       = start_x + i * (btn_w + gap);
        ni.y       = btn_y;
        ni.w       = btn_w;
        ni.h       = btn_h;
        ni.hovered = false;
        m_nav_items.push_back(ni);
    }
}

void HomeScreen::render(Renderer& r, const Theme& t) {
    // Background.
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);

    // ── Header ──────────────────────────────────────────────────────────
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text(std::string("Calcium Client"), t.spacing.lg, 18,
                t.font_size_large, t.accent);
    r.draw_text(std::string("Homebrew Software Center"),
                m_w - r.text_width("Homebrew Software Center", t.font_size_small) - t.spacing.lg,
                24, t.font_size_small, t.text_secondary);
    r.draw_line(0, 64, m_w, 64, t.border);

    // ── "Featured Apps" section ──────────────────────────────────────────
    int section_y = 80;
    r.draw_text(std::string("Featured Apps"),
                t.spacing.lg, section_y, t.font_size_title, t.text_primary);
    section_y += r.text_height(t.font_size_title) + t.spacing.sm;

    if (m_featured.empty()) {
        r.draw_text(std::string("No apps found. Configure a repository in Settings."),
                    t.spacing.lg, section_y, t.font_size_body, t.text_secondary);
    } else {
        int card_w = 200, card_h = 220, gap = t.spacing.md;
        int cards_per_row = (m_w - 2 * t.spacing.lg) / (card_w + gap);
        for (int i = 0; i < static_cast<int>(m_featured.size()); ++i) {
            const auto& fi = m_featured[i];
            int col  = i % cards_per_row;
            int row  = i / cards_per_row;
            int cx   = t.spacing.lg + col * (card_w + gap);
            int cy   = section_y + row * (card_h + gap);

            bool hov = (i == m_hovered_idx);
            Color bg = hov ? t.bg_hover : t.bg_card;
            r.fill_rounded_rect(cx, cy, card_w, card_h, t.corner_radius, bg);
            r.draw_rounded_rect(cx, cy, card_w, card_h, t.corner_radius, t.border);

            // Icon area.
            int icon = 56;
            r.fill_rounded_rect(cx + (card_w - icon) / 2, cy + t.spacing.md,
                                icon, icon, 8, t.bg_selected);
            if (!fi.name.empty()) {
                std::string init(1, fi.name[0]);
                r.draw_text_centered(init, cx + (card_w - icon) / 2,
                                     cy + t.spacing.md + (icon - r.text_height(t.font_size_large)) / 2,
                                     icon, t.font_size_large, t.accent);
            }

            int ty = cy + t.spacing.md + icon + t.spacing.sm;
            r.draw_text_centered(fi.name, cx, ty, card_w, t.font_size_body, t.text_primary);
            ty += r.text_height(t.font_size_body) + 4;
            r.draw_text_centered(fi.author, cx, ty, card_w, t.font_size_small, t.text_secondary);
            ty += r.text_height(t.font_size_small) + 4;
            r.draw_text_clipped(fi.description, cx + 8, ty, card_w - 16,
                                t.font_size_small, t.text_secondary);

            if (fi.installed) {
                int bw = r.text_width("INSTALLED", t.font_size_small) + 8;
                r.fill_rounded_rect(cx + card_w - bw - 4, cy + 4, bw, 18, 3, t.success);
                r.draw_text_centered("INSTALLED", cx + card_w - bw - 4, cy + 6,
                                     bw, t.font_size_small, Color{15,15,20});
            }
        }
    }

    // ── Quick nav buttons ────────────────────────────────────────────────
    for (const auto& ni : m_nav_items) {
        Color bg = ni.hovered ? t.accent_hover : t.accent;
        r.fill_rounded_rect(ni.x, ni.y, ni.w, ni.h, t.corner_radius, bg);
        int ty = ni.y + (ni.h - r.text_height(t.font_size_body)) / 2;
        r.draw_text_centered(ni.label, ni.x, ty, ni.w, t.font_size_body, t.text_primary);
    }
}

#ifdef CALCIUM_DESKTOP_SDL
bool HomeScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_MOUSEMOTION) {
        int mx = ev.motion.x, my = ev.motion.y;
        for (auto& ni : m_nav_items) {
            ni.hovered = (mx >= ni.x && mx < ni.x + ni.w &&
                          my >= ni.y && my < ni.y + ni.h);
        }
        // Featured card hover.
        m_hovered_idx = -1;
        int card_w = 200, card_h = 220, gap = 16;
        int cards_per_row = (m_w - 2 * 24) / (card_w + gap);
        int section_y = 80 + 28 + 8;
        for (int i = 0; i < static_cast<int>(m_featured.size()); ++i) {
            int cx = 24 + (i % cards_per_row) * (card_w + gap);
            int cy = section_y + (i / cards_per_row) * (card_h + gap);
            if (mx >= cx && mx < cx + card_w && my >= cy && my < cy + card_h) {
                m_hovered_idx = i;
                break;
            }
        }
    } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int mx = ev.button.x, my = ev.button.y;
        for (const auto& ni : m_nav_items) {
            if (mx >= ni.x && mx < ni.x + ni.w && my >= ni.y && my < ni.y + ni.h) {
                if (m_nav) m_nav(ni.target);
                return true;
            }
        }
        // Featured card click.
        int card_w = 200, card_h = 220, gap = 16;
        int cards_per_row = std::max(1, (m_w - 48) / (card_w + gap));
        int section_y = 116;
        for (int i = 0; i < static_cast<int>(m_featured.size()); ++i) {
            int cx = 24 + (i % cards_per_row) * (card_w + gap);
            int cy = section_y + (i / cards_per_row) * (card_h + gap);
            if (mx >= cx && mx < cx + card_w && my >= cy && my < cy + card_h) {
                if (m_open_app) m_open_app(m_featured[i].app_id);
                return true;
            }
        }
    } else if (ev.type == SDL_WINDOWEVENT &&
               ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
