#pragma once

#include <cstdint>
#include <string>

#ifdef CALCIUM_DESKTOP_SDL
#  include <SDL2/SDL.h>
#endif

namespace calcium::ui {

/// RGBA colour value (each channel 0–255).
struct Color {
    uint8_t r, g, b, a;

    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

#ifdef CALCIUM_DESKTOP_SDL
    SDL_Color sdl() const { return {r, g, b, a}; }
#endif

    Color with_alpha(uint8_t alpha) const { return {r, g, b, alpha}; }
};

/// Spacing and sizing constants.
struct Spacing {
    int xs  =  4;
    int sm  =  8;
    int md  = 16;
    int lg  = 24;
    int xl  = 32;
    int xxl = 48;
};

/// All visual constants for the dark minimalist theme.
struct Theme {
    // ── Background layers ────────────────────────────────────────────────
    Color bg_primary    {  15,  15,  20, 255 };  // Near-black main bg
    Color bg_secondary  {  22,  22,  30, 255 };  // Slightly lighter panels
    Color bg_card       {  28,  28,  38, 255 };  // App cards, list items
    Color bg_hover      {  38,  38,  52, 255 };  // Hover/focus state
    Color bg_selected   {  45,  45,  65, 255 };  // Selected item
    Color bg_overlay    {   0,   0,   0, 180 };  // Modal backdrop

    // ── Accent ───────────────────────────────────────────────────────────
    Color accent        {  82, 130, 255, 255 };  // Primary accent (blue)
    Color accent_hover  { 102, 150, 255, 255 };
    Color accent_dim    {  82, 130, 255, 120 };

    // ── Status colours ────────────────────────────────────────────────────
    Color success       {  60, 200, 100, 255 };
    Color warning       { 240, 180,  40, 255 };
    Color error_color   { 220,  60,  60, 255 };
    Color info          {  80, 180, 220, 255 };

    // ── Text ─────────────────────────────────────────────────────────────
    Color text_primary  { 230, 230, 235, 255 };
    Color text_secondary{ 150, 150, 165, 255 };
    Color text_disabled {  80,  80,  95, 255 };
    Color text_accent   {  82, 130, 255, 255 };

    // ── Borders ───────────────────────────────────────────────────────────
    Color border        {  48,  48,  65, 255 };
    Color border_focus  {  82, 130, 255, 255 };

    // ── Scrollbar ─────────────────────────────────────────────────────────
    Color scrollbar_bg  {  28,  28,  38, 255 };
    Color scrollbar_fg  {  70,  70,  90, 255 };

    // ── Progress bar ──────────────────────────────────────────────────────
    Color progress_bg   {  38,  38,  52, 255 };
    Color progress_fill { 82, 130, 255, 255 };

    // ── Spacing ───────────────────────────────────────────────────────────
    Spacing spacing;

    // ── Typography sizes (point sizes for SDL_ttf) ────────────────────────
    int font_size_small  = 13;
    int font_size_body   = 16;
    int font_size_sub    = 18;
    int font_size_title  = 22;
    int font_size_large  = 28;
    int font_size_hero   = 36;

    // ── Layout ────────────────────────────────────────────────────────────
    int sidebar_width    = 220;
    int card_width       = 240;
    int card_height      = 300;
    int icon_size_small  =  48;
    int icon_size_large  = 128;
    int corner_radius    =   6;
    int scrollbar_width  =   6;
    int border_width     =   1;

    // ── Animation ─────────────────────────────────────────────────────────
    int anim_duration_ms = 120;

    static const Theme& dark();
};

} // namespace calcium::ui
