#pragma once

#include "Theme.hpp"
#include <string>
#include <memory>
#include <unordered_map>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#  include <SDL2/SDL_ttf.h>
#endif

namespace calcium::ui {

/// Wraps SDL_Renderer with higher-level drawing helpers used by all widgets.
/// Owns font resources and provides a simple text-rendering API.
class Renderer {
public:
#ifdef CALCIUM_DESKTOP_SDL
    explicit Renderer(SDL_Renderer* sdl_renderer);
    ~Renderer();

    SDL_Renderer* sdl() const { return m_renderer; }

    // ── State ──────────────────────────────────────────────────────────────
    void set_clip(int x, int y, int w, int h);
    void clear_clip();
    void set_viewport(int x, int y, int w, int h);
    void clear_viewport();

    // ── Drawing primitives ─────────────────────────────────────────────────
    void clear(Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    void draw_rect(int x, int y, int w, int h, Color c, int thickness = 1);
    void fill_rounded_rect(int x, int y, int w, int h, int radius, Color c);
    void draw_rounded_rect(int x, int y, int w, int h, int radius, Color c, int thickness = 1);
    void draw_line(int x1, int y1, int x2, int y2, Color c);
    void fill_circle(int cx, int cy, int radius, Color c);

    // ── Progress bar ───────────────────────────────────────────────────────
    void draw_progress_bar(int x, int y, int w, int h,
                           float progress, Color bg, Color fill, int radius = 3);

    // ── Text ───────────────────────────────────────────────────────────────
    /// Measure text width in pixels at the given point size.
    int  text_width(const std::string& text, int pt_size) const;
    int  text_height(int pt_size) const;

    /// Draw text at (x, y). Returns the pixel width rendered.
    int  draw_text(const std::string& text, int x, int y, int pt_size, Color c);

    /// Draw text centred horizontally within [x, x+w].
    void draw_text_centered(const std::string& text, int x, int y, int w,
                            int pt_size, Color c);

    /// Draw text truncated with "…" if wider than max_width pixels.
    void draw_text_clipped(const std::string& text, int x, int y,
                           int max_width, int pt_size, Color c);

private:
    TTF_Font* get_font(int pt_size) const;

    /// Midpoint-circle algorithm used for rounded rects and circles.
    void fill_circle_impl(int cx, int cy, int r, Color c);

    SDL_Renderer*                               m_renderer = nullptr;
    mutable std::unordered_map<int, TTF_Font*>  m_fonts;
    std::string                                 m_font_path;
#else
    // Headless stub — all methods are no-ops when SDL is not available.
    explicit Renderer(void*) {}
    void clear(Color)                                            {}
    void fill_rect(int,int,int,int,Color)                        {}
    void draw_rect(int,int,int,int,Color,int=1)                  {}
    void fill_rounded_rect(int,int,int,int,int,Color)            {}
    void draw_rounded_rect(int,int,int,int,int,Color,int=1)      {}
    void draw_line(int,int,int,int,Color)                        {}
    void draw_progress_bar(int,int,int,int,float,Color,Color,int=3) {}
    int  text_width(const std::string&, int) const               { return 0; }
    int  text_height(int) const                                  { return 0; }
    int  draw_text(const std::string&,int,int,int,Color)         { return 0; }
    void draw_text_centered(const std::string&,int,int,int,int,Color) {}
    void draw_text_clipped(const std::string&,int,int,int,int,Color)  {}
    void set_clip(int,int,int,int)  {}
    void clear_clip()               {}
    void set_viewport(int,int,int,int) {}
    void clear_viewport()           {}
#endif
};

} // namespace calcium::ui
