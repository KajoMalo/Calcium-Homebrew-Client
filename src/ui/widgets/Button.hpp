#pragma once

#include "../Theme.hpp"
#include "../Renderer.hpp"
#include <string>
#include <functional>

namespace calcium::ui::widgets {

enum class ButtonStyle { Primary, Secondary, Danger, Ghost };

class Button {
public:
    Button(std::string label, int x, int y, int w, int h,
           ButtonStyle style = ButtonStyle::Primary);

    void set_label(std::string label) { m_label = std::move(label); }
    void set_enabled(bool enabled)    { m_enabled = enabled; }
    void set_position(int x, int y)   { m_x = x; m_y = y; }
    void set_size(int w, int h)       { m_w = w; m_h = h; }
    void on_click(std::function<void()> cb) { m_on_click = std::move(cb); }

    void render(Renderer& r, const Theme& theme) const;

    /// Returns true if the point (px, py) is inside and the button is enabled.
    bool handle_mouse_move(int px, int py);
    /// Returns true if the click was consumed.
    bool handle_mouse_click(int px, int py);

    int x() const { return m_x; }
    int y() const { return m_y; }
    int w() const { return m_w; }
    int h() const { return m_h; }

private:
    bool contains(int px, int py) const;

    std::string             m_label;
    int                     m_x, m_y, m_w, m_h;
    ButtonStyle             m_style;
    bool                    m_enabled  = true;
    bool                    m_hovered  = false;
    std::function<void()>   m_on_click;
};

} // namespace calcium::ui::widgets
