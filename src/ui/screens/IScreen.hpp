#pragma once

#include "../Renderer.hpp"
#include "../Theme.hpp"

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

/// Abstract base for all application screens.
class IScreen {
public:
    virtual ~IScreen() = default;

    /// Called once when the screen is pushed onto the stack.
    virtual void on_enter() {}
    /// Called once when the screen is popped.
    virtual void on_exit()  {}
    /// Called every frame before render.
    virtual void update(float dt_secs) { (void)dt_secs; }
    /// Render the screen contents.
    virtual void render(Renderer& r, const Theme& t) = 0;

#ifdef CALCIUM_DESKTOP_SDL
    /// Return true if the event was fully consumed.
    virtual bool handle_event(const SDL_Event& event) { (void)event; return false; }
#endif
};

} // namespace calcium::ui::screens
