#pragma once

#include "../Theme.hpp"
#include "../Renderer.hpp"
#include <string>

namespace calcium::ui::widgets {

enum class LabelAlign { Left, Center, Right };

class Label {
public:
    Label(std::string text, int x, int y, int max_width = 0,
          int pt_size = 0, LabelAlign align = LabelAlign::Left);

    void set_text(std::string t)       { m_text = std::move(t); }
    void set_color(Color c)            { m_color = c; m_has_color = true; }
    void set_pt_size(int s)            { m_pt_size = s; }
    void set_position(int x, int y)    { m_x = x; m_y = y; }
    void set_max_width(int w)          { m_max_width = w; }

    const std::string& text() const { return m_text; }

    void render(Renderer& r, const Theme& theme) const;

private:
    std::string m_text;
    int         m_x, m_y;
    int         m_max_width;
    int         m_pt_size;
    LabelAlign  m_align;
    Color       m_color{230, 230, 235};
    bool        m_has_color = false;
};

} // namespace calcium::ui::widgets
