#include "CatalogScreen.hpp"
#include <algorithm>
#include <set>

#ifdef CALCIUM_DESKTOP_SDL
#include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

CatalogScreen::CatalogScreen(core::AppManager& app_manager, AppCallback open_app_cb)
    : m_app_manager(app_manager)
    , m_open_app(std::move(open_app_cb))
{}

void CatalogScreen::on_enter() {
    m_all_apps = m_app_manager.catalog();
    // Build unique category list.
    std::set<std::string> cats;
    for (const auto& a : m_all_apps) if (!a.category.empty()) cats.insert(a.category);
    m_categories.clear();
    m_categories.push_back("All");
    for (const auto& c : cats) m_categories.push_back(c);
    m_active_category.clear();
    apply_filter();
    m_dirty = true;
}

void CatalogScreen::apply_filter() {
    if (!m_search_query.empty()) {
        m_filtered = m_app_manager.search(m_search_query);
    } else if (!m_active_category.empty() && m_active_category != "All") {
        m_filtered = m_app_manager.catalog_by_category(m_active_category);
    } else {
        m_filtered = m_all_apps;
    }
    m_dirty = true;
    m_scroll_y = 0;
}

void CatalogScreen::rebuild_cards(const Theme& t) {
    m_cards.clear();
    int cols = std::max(1, (m_w - t.spacing.lg * 2 + m_grid_gap) /
                             (m_card_w + m_grid_gap));
    for (int i = 0; i < static_cast<int>(m_filtered.size()); ++i) {
        int col = i % cols;
        int row = i / cols;
        int cx  = t.spacing.lg + col * (m_card_w + m_grid_gap);
        int cy  = 130 + row * (m_card_h + m_grid_gap);
        widgets::AppCard card(cx, cy, m_card_w, m_card_h);
        card.set_metadata(m_filtered[i]);
        card.on_click([this](const std::string& id) {
            if (m_open_app) m_open_app(id);
        });
        m_cards.push_back(std::move(card));
    }
    m_dirty = false;
}

void CatalogScreen::update(float /*dt*/) {
    // Refresh app states (install flag) every frame for responsiveness.
    for (auto& app : m_filtered) {
        app.is_installed      = m_app_manager.is_installed(app.id);
        app.installed_version = m_app_manager.installed_version(app.id);
    }
    if (m_dirty) rebuild_cards(Theme::dark());
}

void CatalogScreen::render(Renderer& r, const Theme& t) {
    if (m_dirty) rebuild_cards(t);

    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text("Catalog", t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_line(0, 64, m_w, 64, t.border);

    // ── Search box ───────────────────────────────────────────────────────
    int sb_x = t.spacing.lg, sb_y = 72, sb_w = 320, sb_h = 36;
    r.fill_rounded_rect(sb_x, sb_y, sb_w, sb_h, t.corner_radius,
                        m_search_focused ? t.bg_hover : t.bg_card);
    r.draw_rounded_rect(sb_x, sb_y, sb_w, sb_h, t.corner_radius,
                        m_search_focused ? t.border_focus : t.border);
    std::string display = m_search_query.empty() ? "Search apps..." : m_search_query;
    Color query_col = m_search_query.empty() ? t.text_disabled : t.text_primary;
    r.draw_text_clipped(display, sb_x + t.spacing.sm, sb_y + 10, sb_w - 16,
                        t.font_size_body, query_col);

    // ── Category filter pills ─────────────────────────────────────────────
    int pill_x = sb_x + sb_w + t.spacing.md;
    int pill_y = sb_y;
    m_cat_buttons.clear();
    for (const auto& cat : m_categories) {
        int pw = r.text_width(cat, t.font_size_small) + t.spacing.md;
        bool active = (cat == m_active_category || (cat == "All" && m_active_category.empty()));
        Color pill_bg = active ? t.accent : (t.bg_card);
        r.fill_rounded_rect(pill_x, pill_y, pw, sb_h, t.corner_radius, pill_bg);
        r.draw_rounded_rect(pill_x, pill_y, pw, sb_h, t.corner_radius, t.border);
        int ty = pill_y + (sb_h - r.text_height(t.font_size_small)) / 2;
        Color pill_text = active ? t.text_primary : t.text_secondary;
        r.draw_text_centered(cat, pill_x, ty, pw, t.font_size_small, pill_text);
        CatBtn cb;
        cb.label  = cat;
        cb.x = pill_x; cb.y = pill_y; cb.w = pw; cb.h = sb_h;
        cb.active = active; cb.hovered = false;
        m_cat_buttons.push_back(cb);
        pill_x += pw + t.spacing.xs;
    }

    // ── App count ─────────────────────────────────────────────────────────
    r.draw_text(std::to_string(m_filtered.size()) + " apps",
                m_w - 80, sb_y + 10, t.font_size_small, t.text_secondary);

    // ── Cards ─────────────────────────────────────────────────────────────
    r.set_clip(0, 128, m_w, m_h - 128);

    for (auto& card : m_cards) {
        // Apply vertical scroll offset.
        int orig_y = card.y();
        const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y - m_scroll_y);
        card.render(r, t);
        const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y);
    }

    r.clear_clip();

    // ── Empty state ───────────────────────────────────────────────────────
    if (m_filtered.empty()) {
        std::string msg = m_search_query.empty()
            ? "No apps in this category."
            : "No results for \"" + m_search_query + "\".";
        r.draw_text_centered(msg, 0, m_h / 2, m_w, t.font_size_body, t.text_secondary);
    }

    // ── Scrollbar ─────────────────────────────────────────────────────────
    if (!m_cards.empty()) {
        int cols = std::max(1, (m_w - t.spacing.lg * 2 + m_grid_gap) /
                                 (m_card_w + m_grid_gap));
        int rows = (static_cast<int>(m_cards.size()) + cols - 1) / cols;
        int total_h = rows * (m_card_h + m_grid_gap) + t.spacing.lg;
        int visible_h = m_h - 128;
        if (total_h > visible_h) {
            float ratio = static_cast<float>(visible_h) / total_h;
            int sb_h2   = static_cast<int>(visible_h * ratio);
            int sb_y2   = 128 + static_cast<int>((visible_h - sb_h2) *
                          static_cast<float>(m_scroll_y) / (total_h - visible_h));
            r.fill_rounded_rect(m_w - t.scrollbar_width - 2, sb_y2,
                                t.scrollbar_width, sb_h2,
                                t.scrollbar_width / 2, t.scrollbar_fg);
        }
    }
}

#ifdef CALCIUM_DESKTOP_SDL
bool CatalogScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
        m_dirty = true;
        return false;
    }
    if (ev.type == SDL_MOUSEWHEEL) {
        int cols = std::max(1, (m_w - 48 + m_grid_gap) / (m_card_w + m_grid_gap));
        int rows = (static_cast<int>(m_cards.size()) + cols - 1) / cols;
        int total_h = rows * (m_card_h + m_grid_gap);
        int max_scroll = std::max(0, total_h - (m_h - 128));
        m_scroll_y = std::clamp(m_scroll_y - ev.wheel.y * 40, 0, max_scroll);
        return true;
    }
    if (ev.type == SDL_MOUSEMOTION) {
        int mx = ev.motion.x, my = ev.motion.y;
        for (auto& card : m_cards) {
            int orig_y = card.y();
            const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y - m_scroll_y);
            const_cast<widgets::AppCard&>(card).handle_mouse_move(mx, my);
            const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y);
        }
        return false;
    }
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        int mx = ev.button.x, my = ev.button.y;
        // Category pills.
        for (const auto& cb : m_cat_buttons) {
            if (mx >= cb.x && mx < cb.x + cb.w && my >= cb.y && my < cb.y + cb.h) {
                m_active_category = (cb.label == "All") ? "" : cb.label;
                m_search_query.clear();
                apply_filter();
                return true;
            }
        }
        // Search box click — toggle focus.
        if (mx >= 24 && mx < 24 + 320 && my >= 72 && my < 108) {
            m_search_focused = true;
            return true;
        } else {
            m_search_focused = false;
        }
        // Cards.
        for (auto& card : m_cards) {
            int orig_y = card.y();
            const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y - m_scroll_y);
            bool consumed = const_cast<widgets::AppCard&>(card).handle_mouse_click(mx, my);
            const_cast<widgets::AppCard&>(card).set_position(card.x(), orig_y);
            if (consumed) return true;
        }
    }
    if (ev.type == SDL_KEYDOWN && m_search_focused) {
        if (ev.key.keysym.sym == SDLK_BACKSPACE && !m_search_query.empty()) {
            m_search_query.pop_back();
            apply_filter();
            return true;
        }
        if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_ESCAPE) {
            m_search_focused = false;
            return true;
        }
    }
    if (ev.type == SDL_TEXTINPUT && m_search_focused) {
        m_search_query += ev.text.text;
        apply_filter();
        return true;
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
