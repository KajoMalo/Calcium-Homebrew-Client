#include "ListView.hpp"
#include <algorithm>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui::widgets {

ListView::ListView(int x, int y, int w, int h)
    : m_x(x), m_y(y), m_w(w), m_h(h) {}

void ListView::set_items(std::vector<ListItem> items) {
    m_items = std::move(items);
    m_scroll_y   = 0;
    m_hovered_i  = -1;
    m_selected_i = -1;
    m_selected_id.clear();
}

void ListView::select(const std::string& id) {
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        if (m_items[i].id == id) {
            m_selected_i  = i;
            m_selected_id = id;
            ensure_visible(i);
            return;
        }
    }
}

int ListView::item_at(int py) const {
    int rel = py - m_y + m_scroll_y;
    if (rel < 0) return -1;
    int index = rel / m_item_h;
    if (index >= static_cast<int>(m_items.size())) return -1;
    return index;
}

void ListView::ensure_visible(int index) {
    int item_top = index * m_item_h;
    int item_bot = item_top + m_item_h;
    if (item_top < m_scroll_y)
        m_scroll_y = item_top;
    else if (item_bot > m_scroll_y + m_h)
        m_scroll_y = item_bot - m_h;
    m_scroll_y = std::max(0, m_scroll_y);
}

bool ListView::handle_mouse_move(int px, int py) {
    if (px < m_x || px > m_x + m_w || py < m_y || py > m_y + m_h) {
        m_hovered_i = -1;
        return false;
    }
    m_hovered_i = item_at(py);
    return true;
}

bool ListView::handle_mouse_click(int px, int py) {
    if (px < m_x || px > m_x + m_w || py < m_y || py > m_y + m_h) return false;
    int idx = item_at(py);
    if (idx < 0) return false;
    m_selected_i  = idx;
    m_selected_id = m_items[idx].id;
    if (m_on_select) m_on_select(m_selected_id);
    return true;
}

bool ListView::handle_mouse_wheel(int delta) {
    int max_scroll = std::max(0,
        static_cast<int>(m_items.size()) * m_item_h - m_h);
    m_scroll_y = std::clamp(m_scroll_y - delta * 30, 0, max_scroll);
    return true;
}

bool ListView::handle_key(int sdl_key) {
#ifdef CALCIUM_DESKTOP_SDL
    if (sdl_key == SDLK_UP || sdl_key == SDLK_DOWN) {
        int next = m_selected_i + (sdl_key == SDLK_DOWN ? 1 : -1);
        next = std::clamp(next, 0, static_cast<int>(m_items.size()) - 1);
        m_selected_i  = next;
        m_selected_id = m_items[next].id;
        ensure_visible(next);
        if (m_on_select) m_on_select(m_selected_id);
        return true;
    }
#endif
    return false;
}

void ListView::render(Renderer& r, const Theme& t) const {
    r.set_clip(m_x, m_y, m_w, m_h);

    int first = m_scroll_y / m_item_h;
    int last  = std::min(static_cast<int>(m_items.size()),
                          first + m_h / m_item_h + 2);

    for (int i = first; i < last; ++i) {
        const auto& item = m_items[i];
        int iy = m_y + i * m_item_h - m_scroll_y;

        Color bg = t.bg_secondary;
        if (i == m_selected_i)  bg = t.bg_selected;
        else if (i == m_hovered_i) bg = t.bg_hover;

        r.fill_rect(m_x, iy, m_w, m_item_h, bg);

        // Primary text.
        int text_x  = m_x + t.spacing.md;
        int text_y1 = iy + t.spacing.sm;
        r.draw_text_clipped(item.primary, text_x, text_y1,
                            m_w - t.spacing.md * 2 - 80,
                            t.font_size_body, t.text_primary);

        // Secondary text.
        int text_y2 = text_y1 + r.text_height(t.font_size_body) + t.spacing.xs;
        r.draw_text_clipped(item.secondary, text_x, text_y2,
                            m_w - t.spacing.md * 2,
                            t.font_size_small, t.text_secondary);

        // Badge.
        if (!item.badge.empty()) {
            int bw = r.text_width(item.badge, t.font_size_small) + t.spacing.sm;
            int bx = m_x + m_w - bw - t.spacing.md;
            int by = iy + (m_item_h - 20) / 2;
            r.fill_rounded_rect(bx, by, bw, 20, 3, t.accent_dim);
            r.draw_text_centered(item.badge, bx, by + 3, bw,
                                 t.font_size_small, t.text_primary);
        }

        // Separator line.
        r.draw_line(m_x, iy + m_item_h - 1, m_x + m_w, iy + m_item_h - 1, t.border);
    }

    // Scrollbar.
    int total_h = static_cast<int>(m_items.size()) * m_item_h;
    if (total_h > m_h) {
        float ratio    = static_cast<float>(m_h) / total_h;
        int   sb_h     = static_cast<int>(m_h * ratio);
        int   sb_y     = m_y + static_cast<int>((m_h - sb_h) *
                         static_cast<float>(m_scroll_y) / (total_h - m_h));
        int   sb_x     = m_x + m_w - t.scrollbar_width;
        r.fill_rounded_rect(sb_x, sb_y, t.scrollbar_width, sb_h,
                            t.scrollbar_width / 2, t.scrollbar_fg);
    }

    r.clear_clip();
}

} // namespace calcium::ui::widgets
