#pragma once

#include "../Theme.hpp"
#include "../Renderer.hpp"
#include "../../repository/AppMetadata.hpp"
#include <functional>
#include <string>

namespace calcium::ui::widgets {

/// Card widget for the catalog grid view.
class AppCard {
public:
    AppCard(int x, int y, int w, int h);

    void set_metadata(const repository::AppMetadata& meta);
    void set_position(int x, int y) { m_x = x; m_y = y; }

    void on_click(std::function<void(const std::string& app_id)> cb) {
        m_on_click = std::move(cb);
    }

    void render(Renderer& r, const Theme& t) const;

    bool handle_mouse_move(int px, int py);
    bool handle_mouse_click(int px, int py);

private:
    bool contains(int px, int py) const;

    int  m_x, m_y, m_w, m_h;
    bool m_hovered = false;

    std::string m_app_id;
    std::string m_name;
    std::string m_author;
    std::string m_version;
    std::string m_category;
    std::string m_description;
    bool        m_installed  = false;
    bool        m_has_update = false;

    std::function<void(const std::string&)> m_on_click;
};

} // namespace calcium::ui::widgets
