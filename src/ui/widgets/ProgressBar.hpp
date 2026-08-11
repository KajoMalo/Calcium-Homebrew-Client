#pragma once

#include "../Theme.hpp"
#include "../Renderer.hpp"
#include <string>

namespace calcium::ui::widgets {

class ProgressBar {
public:
    ProgressBar(int x, int y, int w, int h);

    void set_progress(float p)             { m_progress = p; }
    void set_label(std::string l)          { m_label = std::move(l); }
    void set_position(int x, int y)        { m_x = x; m_y = y; }
    void set_indeterminate(bool v)         { m_indeterminate = v; }

    float progress() const { return m_progress; }

    void render(Renderer& r, const Theme& t) const;
    void update(float dt_secs);   // Advance indeterminate animation

private:
    int         m_x, m_y, m_w, m_h;
    float       m_progress      = 0.0f;
    std::string m_label;
    bool        m_indeterminate = false;
    mutable float m_anim_offset = 0.0f;
};

} // namespace calcium::ui::widgets
