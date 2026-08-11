#pragma once

#include "IScreen.hpp"
#include "../../core/AppManager.hpp"
#include <functional>
#include <vector>
#include <string>

namespace calcium::ui::screens {

class HomeScreen final : public IScreen {
public:
    using NavCallback = std::function<void(const std::string& screen)>;
    using AppCallback = std::function<void(const std::string& app_id)>;

    HomeScreen(core::AppManager& app_manager,
               NavCallback nav_cb,
               AppCallback open_app_cb);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;

#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    void refresh_featured();

    core::AppManager&   m_app_manager;
    NavCallback         m_nav;
    AppCallback         m_open_app;

    struct FeaturedItem {
        std::string app_id;
        std::string name;
        std::string author;
        std::string description;
        std::string category;
        bool        installed = false;
    };
    std::vector<FeaturedItem> m_featured;
    int  m_featured_scroll = 0;
    int  m_hovered_idx     = -1;

    struct NavItem { std::string label; std::string target; int x, y, w, h; bool hovered; };
    std::vector<NavItem> m_nav_items;
    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
