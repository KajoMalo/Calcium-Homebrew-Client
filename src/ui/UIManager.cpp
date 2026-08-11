#include "UIManager.hpp"
#include "../core/Application.hpp"
#include "screens/HomeScreen.hpp"
#include "screens/CatalogScreen.hpp"
#include "screens/AppDetailScreen.hpp"
#include "screens/InstalledScreen.hpp"
#include "screens/DownloadsScreen.hpp"
#include "screens/SettingsScreen.hpp"
#include "screens/AboutScreen.hpp"
#include "../logging/Logger.hpp"

#include <algorithm>
#include <stdexcept>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui {

static constexpr std::string_view TAG       = "UIManager";
static constexpr int              SIDEBAR_W = 200;

// ─── Construction ─────────────────────────────────────────────────────────────

UIManager::UIManager(core::Application& app, int width, int height)
    : m_app(app), m_width(width), m_height(height)
{}

UIManager::~UIManager() = default;

// ─── Init ─────────────────────────────────────────────────────────────────────

bool UIManager::init() {
#ifdef CALCIUM_DESKTOP_SDL
    // Retrieve the SDL_Renderer created by DesktopPlatform.
    // We cast through the platform to get the SDL_Renderer pointer.
    // DesktopPlatform exposes it via a public accessor.
    // Since IPlatform doesn't expose it, we use the fact that in desktop mode
    // the renderer is the SDL_Renderer embedded in DesktopPlatform.
    // We create a secondary renderer wrapper here owned by the UIManager.
    // In practice the renderer is obtained from the platform — for desktop
    // builds DesktopPlatform::sdl_renderer() would return it. Since we control
    // the platform, we just create a dedicated one for UI use from the same window.
    // For this implementation we retrieve it via SDL directly.
    SDL_Renderer* sdl_r = nullptr;
    SDL_Window*   win   = SDL_GL_GetCurrentWindow();
    if (!win) {
        // Fall back: get the first window.
        win = reinterpret_cast<SDL_Window*>(SDL_GetWindowFromID(1));
    }
    if (win) {
        sdl_r = SDL_GetRenderer(win);
    }
    if (!sdl_r) {
        logging::Logger::instance().warning(TAG,
            "Could not obtain SDL_Renderer from window — creating a fallback.");
        // Last resort: create a headless renderer for text sizing only.
        // This path is taken in unit tests / headless CI.
    }
    m_renderer = std::make_unique<Renderer>(sdl_r);
#else
    m_renderer = std::make_unique<Renderer>(nullptr);
#endif

    m_theme = Theme::dark();
    build_sidebar_items();

    // ── Construct screens ─────────────────────────────────────────────────
    auto nav      = [this](const std::string& id) { navigate(id); };
    auto open_app = [this](const std::string& id) { open_app(id); };
    auto back     = [this]()                        { go_back();  };

    auto& am  = m_app.app_manager();
    auto& lnc = m_app.launcher();
    auto& cfg = m_app.config();
    auto& plt = m_app.platform();

    m_screens["home"] = std::make_shared<screens::HomeScreen>(am, nav, open_app);

    m_screens["catalog"] = std::make_shared<screens::CatalogScreen>(am, open_app);

    m_screens["detail"] = std::make_shared<screens::AppDetailScreen>(am, lnc, back);

    m_screens["installed"] = std::make_shared<screens::InstalledScreen>(am, lnc, open_app);

    m_screens["downloads"] = std::make_shared<screens::DownloadsScreen>(am);

    m_screens["settings"] = std::make_shared<screens::SettingsScreen>(
        cfg, [](const config::AppConfig&) {});

    m_screens["about"] = std::make_shared<screens::AboutScreen>(plt);

    // Start on the home screen.
    navigate("home");

#ifdef CALCIUM_DESKTOP_SDL
    SDL_StartTextInput();
#endif
    m_last_ticks = SDL_GetTicks();

    logging::Logger::instance().info(TAG, "UI initialised.");
    return true;
}

// ─── Navigation ───────────────────────────────────────────────────────────────

void UIManager::build_sidebar_items() {
    m_sidebar_items = {
        {"home",      "Home",      "H"},
        {"catalog",   "Catalog",   "C"},
        {"installed", "Installed", "I"},
        {"downloads", "Downloads", "D"},
        {"settings",  "Settings",  "S"},
        {"about",     "About",     "?"},
    };
}

void UIManager::navigate(const std::string& screen_id) {
    if (m_screens.find(screen_id) == m_screens.end()) {
        logging::Logger::instance().warning(TAG, "Unknown screen: " + screen_id);
        return;
    }
    if (!m_current_id.empty() && m_current_id != screen_id) {
        m_history.push_back(m_current_id);
        if (m_history.size() > 20) m_history.erase(m_history.begin());
        m_screens[m_current_id]->on_exit();
    }
    m_current_id = screen_id;
    m_screens[m_current_id]->on_enter();
    logging::Logger::instance().debug(TAG, "Navigated to: " + screen_id);
}

void UIManager::open_app(const std::string& app_id) {
    // Set the app id on the detail screen before navigating to it.
    auto it = m_screens.find("detail");
    if (it != m_screens.end()) {
        auto* detail = dynamic_cast<screens::AppDetailScreen*>(it->second.get());
        if (detail) detail->set_app_id(app_id);
    }
    navigate("detail");
}

void UIManager::go_back() {
    if (!m_history.empty()) {
        std::string prev = m_history.back();
        m_history.pop_back();
        if (!m_current_id.empty()) m_screens[m_current_id]->on_exit();
        m_current_id = prev;
        m_screens[m_current_id]->on_enter();
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────

void UIManager::update() {
#ifdef CALCIUM_DESKTOP_SDL
    // Calculate delta time.
    uint32_t now = SDL_GetTicks();
    float dt = static_cast<float>(now - m_last_ticks) / 1000.0f;
    m_last_ticks = now;

    // Pump events.
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        handle_event(ev);
    }

    if (!m_current_id.empty()) {
        m_screens[m_current_id]->update(dt);
    }
#endif
}

#ifdef CALCIUM_DESKTOP_SDL
void UIManager::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_QUIT) {
        m_app.request_exit();
        return;
    }
    if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
        go_back();
        return;
    }

    // Update window dimensions on resize.
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_width  = ev.window.data1;
        m_height = ev.window.data2;
    }

    // Sidebar click — only for non-detail screens.
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        handle_sidebar_click(ev.button.x, ev.button.y);
    }
    if (ev.type == SDL_MOUSEMOTION) {
        int mx = ev.motion.x, my = ev.motion.y;
        for (auto& item : m_sidebar_items) {
            // Sidebar items are 40px tall starting at y=80.
            int idx = static_cast<int>(&item - m_sidebar_items.data());
            int iy  = 80 + idx * 48;
            item.hovered = (mx >= 0 && mx < SIDEBAR_W &&
                            my >= iy && my < iy + 40);
        }
    }

    // Forward to current screen (offset x for sidebar).
    if (!m_current_id.empty()) {
        SDL_Event adjusted = ev;
        // Offset mouse events past the sidebar.
        if (adjusted.type == SDL_MOUSEBUTTONDOWN ||
            adjusted.type == SDL_MOUSEBUTTONUP) {
            adjusted.button.x -= SIDEBAR_W;
        } else if (adjusted.type == SDL_MOUSEMOTION) {
            adjusted.motion.x -= SIDEBAR_W;
        }
        m_screens[m_current_id]->handle_event(adjusted);
    }
}
#endif

void UIManager::handle_sidebar_click(int mx, int my) {
    if (mx >= SIDEBAR_W) return; // clicked in content area
    for (int i = 0; i < static_cast<int>(m_sidebar_items.size()); ++i) {
        int iy = 80 + i * 48;
        if (mx >= 0 && mx < SIDEBAR_W && my >= iy && my < iy + 40) {
            navigate(m_sidebar_items[i].id);
            return;
        }
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

void UIManager::render_sidebar() {
    const Theme& t = m_theme;

    // Sidebar background.
    m_renderer->fill_rect(0, 0, SIDEBAR_W, m_height, t.bg_secondary);
    m_renderer->draw_line(SIDEBAR_W, 0, SIDEBAR_W, m_height, t.border);

    // App logo / name at top.
    m_renderer->fill_rounded_rect(12, 12, 40, 40, 8, t.bg_card);
    m_renderer->draw_text_centered("Ca", 12, 12 + (40 - m_renderer->text_height(t.font_size_sub)) / 2,
                                    40, t.font_size_sub, t.accent);
    m_renderer->draw_text("Calcium", 60, 22, t.font_size_body, t.text_primary);

    m_renderer->draw_line(0, 64, SIDEBAR_W, 64, t.border);

    // Nav items.
    for (int i = 0; i < static_cast<int>(m_sidebar_items.size()); ++i) {
        const auto& item = m_sidebar_items[i];
        int iy = 80 + i * 48;
        bool active = (item.id == m_current_id);

        Color bg = active ? t.bg_selected : (item.hovered ? t.bg_hover : t.bg_secondary);
        m_renderer->fill_rect(0, iy, SIDEBAR_W, 40, bg);

        // Active indicator bar.
        if (active) {
            m_renderer->fill_rect(0, iy, 3, 40, t.accent);
        }

        // Icon.
        m_renderer->draw_text(item.icon_char, 14, iy + (40 - m_renderer->text_height(t.font_size_body)) / 2,
                               t.font_size_body, active ? t.accent : t.text_secondary);

        // Label.
        Color label_c = active ? t.text_primary : t.text_secondary;
        m_renderer->draw_text(item.label, 38, iy + (40 - m_renderer->text_height(t.font_size_body)) / 2,
                               t.font_size_body, label_c);
    }

    // Version at bottom.
    m_renderer->draw_text("v1.0.0",
                           8, m_height - 24,
                           t.font_size_small, t.text_disabled);
}

void UIManager::render() {
    if (!m_renderer) return;

    m_renderer->clear(m_theme.bg_primary);

#ifdef CALCIUM_DESKTOP_SDL
    // Render sidebar in full viewport.
    m_renderer->clear_viewport();
    render_sidebar();

    // Render current screen in content area (offset by sidebar width).
    if (!m_current_id.empty()) {
        m_renderer->set_viewport(SIDEBAR_W, 0, m_width - SIDEBAR_W, m_height);
        m_screens[m_current_id]->render(*m_renderer, m_theme);
        m_renderer->clear_viewport();
    }
#else
    if (!m_current_id.empty()) {
        m_screens[m_current_id]->render(*m_renderer, m_theme);
    }
#endif
}

} // namespace calcium::ui
