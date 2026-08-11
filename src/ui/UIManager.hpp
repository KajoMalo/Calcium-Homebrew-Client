#pragma once

#include "Renderer.hpp"
#include "Theme.hpp"
#include "screens/IScreen.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

// Forward declarations to avoid pulling in Application.hpp from UI headers.
namespace calcium::core  { class Application; }

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui {

/// Owns the screen stack, sidebar navigation, and the per-frame update/render
/// pipeline. All screens are created once and reused.
class UIManager {
public:
    UIManager(core::Application& app, int width, int height);
    ~UIManager();

    /// Create all screens and wire callbacks. Must be called after platform init.
    bool init();

    /// Advance animations / state — call once per frame before render().
    void update();

    /// Render the current screen and the sidebar — call once per frame.
    void render();

    /// Navigate to a named screen (e.g. "home", "catalog", "settings").
    void navigate(const std::string& screen_id);

    /// Push the app-detail screen for a given app id.
    void open_app(const std::string& app_id);

    /// Pop the current screen back to the previous one.
    void go_back();

    const std::string& current_screen_id() const { return m_current_id; }

private:
    void build_sidebar_items();
    void render_sidebar();
    void handle_sidebar_click(int mx, int my);

#ifdef CALCIUM_DESKTOP_SDL
    void handle_event(const SDL_Event& ev);
#endif

    core::Application& m_app;
    int                m_width;
    int                m_height;

    std::unique_ptr<Renderer> m_renderer;
    Theme                     m_theme;

    std::unordered_map<std::string, std::shared_ptr<screens::IScreen>> m_screens;
    std::vector<std::string>  m_history;       ///< Navigation stack.
    std::string               m_current_id;
    std::string               m_pending_nav;   ///< Set during update, applied next frame.

    // Sidebar.
    struct SidebarItem {
        std::string id;
        std::string label;
        std::string icon_char; ///< Single unicode-ish char used as icon.
        bool        hovered = false;
    };
    std::vector<SidebarItem> m_sidebar_items;

    // Timing.
    uint32_t m_last_ticks = 0;

    // App detail: which app id to show.
    std::string m_pending_app_id;
};

} // namespace calcium::ui
