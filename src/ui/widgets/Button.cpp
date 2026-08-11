#include "Button.hpp"

namespace calcium::ui::widgets {

Button::Button(std::string label, int x, int y, int w, int h, ButtonStyle style)
    : m_label(std::move(label)), m_x(x), m_y(y), m_w(w), m_h(h), m_style(style) {}

bool Button::contains(int px, int py) const {
    return px >= m_x && px < m_x + m_w && py >= m_y && py < m_y + m_h;
}

bool Button::handle_mouse_move(int px, int py) {
    m_hovered = contains(px, py) && m_enabled;
    return m_hovered;
}

bool Button::handle_mouse_click(int px, int py) {
    if (!m_enabled || !contains(px, py)) return false;
    if (m_on_click) m_on_click();
    return true;
}

void Button::render(Renderer& r, const Theme& t) const {
    Color bg = t.accent;
    Color border = t.accent;
    Color text_c = t.text_primary;

    switch (m_style) {
        case ButtonStyle::Primary:
            bg     = m_hovered ? t.accent_hover : t.accent;
            border = bg;
            break;
        case ButtonStyle::Secondary:
            bg     = m_hovered ? t.bg_hover : t.bg_card;
            border = t.border;
            break;
        case ButtonStyle::Danger:
            bg     = m_hovered ? Color{240, 80, 80} : t.error_color;
            border = bg;
            break;
        case ButtonStyle::Ghost:
            bg     = m_hovered ? t.bg_hover : t.bg_primary.with_alpha(0);
            border = t.border;
            break;
    }

    if (!m_enabled) {
        bg     = t.bg_card;
        border = t.border;
        text_c = t.text_disabled;
    }

    r.fill_rounded_rect(m_x, m_y, m_w, m_h, t.corner_radius, bg);
    r.draw_rounded_rect(m_x, m_y, m_w, m_h, t.corner_radius, border);

    int text_y = m_y + (m_h - r.text_height(t.font_size_body)) / 2;
    r.draw_text_centered(m_label, m_x, text_y, m_w, t.font_size_body, text_c);
}

} // namespace calcium::ui::widgets
