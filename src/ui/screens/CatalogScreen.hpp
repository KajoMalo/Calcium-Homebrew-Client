#pragma once

#include "IScreen.hpp"
#include "../../core/AppManager.hpp"
#include "../widgets/AppCard.hpp"
#include "../widgets/Button.hpp"
#include <vector>
#include <string>
#include <functional>

namespace calcium::ui::screens {

class CatalogScreen final : public IScreen {
public:
    using AppCallback = std::function<void(const std::string& app_id)>;

    CatalogScreen(core::AppManager& app_manager, AppCallback open_app_cb);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    void rebuild_cards(const Theme& t);
    void apply_filter();

    core::AppManager& m_app_manager;
    AppCallback       m_open_app;

    std::vector<repository::AppMetadata> m_all_apps;
    std::vector<repository::AppMetadata> m_filtered;
    std::vector<widgets::AppCard>        m_cards;

    std::string m_search_query;
    std::string m_active_category;
    std::vector<std::string> m_categories;

    // Category filter buttons.
    struct CatBtn { std::string label; int x, y, w, h; bool hovered; bool active; };
    std::vector<CatBtn> m_cat_buttons;

    // Search box state.
    bool m_search_focused = false;

    int  m_scroll_y     = 0;
    int  m_card_w       = 220;
    int  m_card_h       = 280;
    int  m_grid_gap     = 16;
    int  m_w = 1280, m_h = 720;

    // Rebuild flag.
    bool m_dirty = true;
};

} // namespace calcium::ui::screens
