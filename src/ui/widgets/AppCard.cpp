#include "AppCard.hpp"
#include <algorithm>

namespace calcium::ui::widgets {

AppCard::AppCard(int x, int y, int w, int h)
    : m_x(x), m_y(y), m_w(w), m_h(h) {}

void AppCard::set_metadata(const repository::AppMetadata& meta) {
    m_app_id     = meta.id;
    m_name       = meta.name;
    m_author     = meta.author;
    m_version    = meta.version;
    m_category   = meta.category;
    m_description = meta.description;
    m_installed  = meta.is_installed;
    m_has_update = meta.has_update();
}

bool AppCard::contains(int px, int py) const {
    return px >= m_x && px < m_x + m_w && py >= m_y && py < m_y + m_h;
}

bool AppCard::handle_mouse_move(int px, int py) {
    m_hovered = contains(px, py);
    return m_hovered;
}

bool AppCard::handle_mouse_click(int px, int py) {
    if (!contains(px, py)) return false;
    if (m_on_click) m_on_click(m_app_id);
    return true;
}

void AppCard::render(Renderer& r, const Theme& t) const {
    Color bg = m_hovered ? t.bg_hover : t.bg_card;
    r.fill_rounded_rect(m_x, m_y, m_w, m_h, t.corner_radius, bg);
    r.draw_rounded_rect(m_x, m_y, m_w, m_h, t.corner_radius, t.border);

    // Icon placeholder — a rounded rect with the first letter of the app name.
    int icon_size = t.icon_size_small;
    int icon_x    = m_x + (m_w - icon_size) / 2;
    int icon_y    = m_y + t.spacing.md;
    r.fill_rounded_rect(icon_x, icon_y, icon_size, icon_size,
                        t.corner_radius * 2, t.bg_selected);

    if (!m_name.empty()) {
        std::string initial(1, m_name[0]);
        int lh = r.text_height(t.font_size_large);
        r.draw_text_centered(initial,
                             icon_x, icon_y + (icon_size - lh) / 2,
                             icon_size, t.font_size_large, t.accent);
    }

    int content_y = icon_y + icon_size + t.spacing.sm;
    int content_x = m_x + t.spacing.sm;
    int content_w = m_w - t.spacing.sm * 2;

    // App name.
    r.draw_text_centered(m_name, content_x, content_y,
                         content_w, t.font_size_sub, t.text_primary);
    content_y += r.text_height(t.font_size_sub) + t.spacing.xs;

    // Author.
    r.draw_text_centered(m_author, content_x, content_y,
                         content_w, t.font_size_small, t.text_secondary);
    content_y += r.text_height(t.font_size_small) + t.spacing.xs;

    // Description (single line, clipped).
    r.draw_text_clipped(m_description, content_x, content_y,
                        content_w, t.font_size_small, t.text_secondary);
    content_y += r.text_height(t.font_size_small) + t.spacing.sm;

    // Category badge.
    if (!m_category.empty()) {
        int bw = r.text_width(m_category, t.font_size_small) + t.spacing.sm;
        int bx = m_x + (m_w - bw) / 2;
        r.fill_rounded_rect(bx, content_y, bw, 20, 3, t.bg_secondary);
        r.draw_text_centered(m_category, bx, content_y + 3,
                             bw, t.font_size_small, t.text_secondary);
    }

    // Installed badge (top-right corner).
    if (m_installed) {
        Color badge_col = m_has_update ? t.warning : t.success;
        std::string badge_text = m_has_update ? "UPDATE" : "INSTALLED";
        int bw = r.text_width(badge_text, t.font_size_small) + 8;
        int bx = m_x + m_w - bw - t.spacing.xs;
        int by = m_y + t.spacing.xs;
        r.fill_rounded_rect(bx, by, bw, 18, 3, badge_col);
        r.draw_text_centered(badge_text, bx, by + 2,
                             bw, t.font_size_small, Color{15, 15, 20});
    }
}

} // namespace calcium::ui::widgets
