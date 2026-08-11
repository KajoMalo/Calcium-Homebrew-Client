#include "Label.hpp"

namespace calcium::ui::widgets {

Label::Label(std::string text, int x, int y, int max_width, int pt_size, LabelAlign align)
    : m_text(std::move(text)), m_x(x), m_y(y)
    , m_max_width(max_width), m_pt_size(pt_size), m_align(align)
{}

void Label::render(Renderer& r, const Theme& t) const {
    int pt = m_pt_size > 0 ? m_pt_size : t.font_size_body;
    Color c = m_has_color ? m_color : t.text_primary;

    if (m_max_width > 0) {
        switch (m_align) {
            case LabelAlign::Left:
                r.draw_text_clipped(m_text, m_x, m_y, m_max_width, pt, c);
                break;
            case LabelAlign::Center:
                r.draw_text_centered(m_text, m_x, m_y, m_max_width, pt, c);
                break;
            case LabelAlign::Right: {
                int tw = r.text_width(m_text, pt);
                int rx = m_x + m_max_width - tw;
                r.draw_text(m_text, rx, m_y, pt, c);
                break;
            }
        }
    } else {
        r.draw_text(m_text, m_x, m_y, pt, c);
    }
}

} // namespace calcium::ui::widgets
