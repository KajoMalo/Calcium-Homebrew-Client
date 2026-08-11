#include "ProgressBar.hpp"
#include <cmath>
#include <algorithm>

namespace calcium::ui::widgets {

ProgressBar::ProgressBar(int x, int y, int w, int h)
    : m_x(x), m_y(y), m_w(w), m_h(h) {}

void ProgressBar::update(float dt_secs) {
    if (m_indeterminate) {
        m_anim_offset += dt_secs * 0.8f;
        if (m_anim_offset > 1.0f) m_anim_offset -= 1.0f;
    }
}

void ProgressBar::render(Renderer& r, const Theme& t) const {
    // Track background.
    r.fill_rounded_rect(m_x, m_y, m_w, m_h, m_h / 2, t.progress_bg);

    if (m_indeterminate) {
        // Sliding highlight for indeterminate state.
        int fill_w = m_w / 3;
        int fill_x = m_x + static_cast<int>(m_anim_offset * (m_w + fill_w)) - fill_w;
        fill_x = std::clamp(fill_x, m_x, m_x + m_w);
        int visible_w = std::min(fill_w, (m_x + m_w) - fill_x);
        if (visible_w > 0) {
            r.fill_rounded_rect(fill_x, m_y, visible_w, m_h, m_h / 2, t.accent);
        }
    } else {
        float clamped = std::clamp(m_progress, 0.0f, 1.0f);
        int fill_w = static_cast<int>(std::round(clamped * m_w));
        if (fill_w > 0) {
            r.fill_rounded_rect(m_x, m_y, fill_w, m_h, m_h / 2, t.accent);
        }
    }

    // Label centred above the bar (if set).
    if (!m_label.empty()) {
        int lh = r.text_height(t.font_size_small);
        r.draw_text_centered(m_label,
                             m_x, m_y - lh - t.spacing.xs,
                             m_w, t.font_size_small, t.text_secondary);
    }
}

} // namespace calcium::ui::widgets
