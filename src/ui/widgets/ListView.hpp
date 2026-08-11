#pragma once

#include "../Theme.hpp"
#include "../Renderer.hpp"
#include <vector>
#include <string>
#include <functional>

namespace calcium::ui::widgets {

struct ListItem {
    std::string id;
    std::string primary;    ///< Main text line
    std::string secondary;  ///< Subtitle / metadata line
    std::string badge;      ///< Optional short badge text (e.g. "INSTALLED")
    bool        selected = false;
};

/// Scrollable list with keyboard/mouse navigation and selection callbacks.
class ListView {
public:
    ListView(int x, int y, int w, int h);

    void set_items(std::vector<ListItem> items);
    void set_position(int x, int y) { m_x = x; m_y = y; }
    void set_size(int w, int h)     { m_w = w; m_h = h; }
    void on_select(std::function<void(const std::string& id)> cb) {
        m_on_select = std::move(cb);
    }

    void render(Renderer& r, const Theme& t) const;

    // Input handlers — return true if event was consumed.
    bool handle_mouse_move(int px, int py);
    bool handle_mouse_click(int px, int py);
    bool handle_mouse_wheel(int delta);
    bool handle_key(int sdl_key);

    const std::string& selected_id() const { return m_selected_id; }
    void select(const std::string& id);

    int item_count() const { return static_cast<int>(m_items.size()); }

private:
    int item_at(int py) const;
    void ensure_visible(int index);

    int  m_x, m_y, m_w, m_h;
    int  m_item_h     = 64;
    int  m_scroll_y   = 0;
    int  m_hovered_i  = -1;
    int  m_selected_i = -1;

    std::vector<ListItem>              m_items;
    std::string                        m_selected_id;
    std::function<void(const std::string&)> m_on_select;
};

} // namespace calcium::ui::widgets
