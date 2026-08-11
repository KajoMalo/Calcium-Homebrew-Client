#include "Renderer.hpp"
#include "../logging/Logger.hpp"

#ifdef CALCIUM_DESKTOP_SDL

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace calcium::ui {

static constexpr std::string_view TAG = "Renderer";

// ─── Font search ──────────────────────────────────────────────────────────────

static std::string find_system_font() {
    // Preferred order: embedded resources font, then system fonts.
    const std::vector<std::string> candidates = {
        // Bundled font (placed by CMake install into resources/).
        "resources/themes/font.ttf",
        "./font.ttf",
#if defined(_WIN32)
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/verdana.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/SFNS.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
#else
        // Linux — check common paths for a clean sans-serif font.
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
#endif
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) return p;
    }
    return {};
}

// ─── Construction ─────────────────────────────────────────────────────────────

Renderer::Renderer(SDL_Renderer* sdl_renderer)
    : m_renderer(sdl_renderer)
{
    if (TTF_Init() != 0) {
        logging::Logger::instance().error(TAG,
            std::string("TTF_Init failed: ") + TTF_GetError());
    }
    m_font_path = find_system_font();
    if (m_font_path.empty()) {
        logging::Logger::instance().warning(TAG,
            "No system font found. Text rendering will be unavailable.");
    } else {
        logging::Logger::instance().info(TAG, "Using font: " + m_font_path);
    }
}

Renderer::~Renderer() {
    for (auto& [size, font] : m_fonts) {
        TTF_CloseFont(font);
    }
    TTF_Quit();
}

// ─── Font cache ───────────────────────────────────────────────────────────────

TTF_Font* Renderer::get_font(int pt_size) const {
    auto it = m_fonts.find(pt_size);
    if (it != m_fonts.end()) return it->second;

    if (m_font_path.empty()) return nullptr;

    TTF_Font* font = TTF_OpenFont(m_font_path.c_str(), pt_size);
    if (!font) {
        logging::Logger::instance().error(TAG,
            "TTF_OpenFont failed at size " + std::to_string(pt_size) +
            ": " + TTF_GetError());
        return nullptr;
    }
    m_fonts.emplace(pt_size, font);
    return font;
}

// ─── State ────────────────────────────────────────────────────────────────────

void Renderer::set_clip(int x, int y, int w, int h) {
    SDL_Rect r{x, y, w, h};
    SDL_RenderSetClipRect(m_renderer, &r);
}

void Renderer::clear_clip() {
    SDL_RenderSetClipRect(m_renderer, nullptr);
}

void Renderer::set_viewport(int x, int y, int w, int h) {
    SDL_Rect r{x, y, w, h};
    SDL_RenderSetViewport(m_renderer, &r);
}

void Renderer::clear_viewport() {
    SDL_RenderSetViewport(m_renderer, nullptr);
}

// ─── Primitives ───────────────────────────────────────────────────────────────

void Renderer::clear(Color c) {
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(m_renderer);
}

void Renderer::fill_rect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(m_renderer, &r);
}

void Renderer::draw_rect(int x, int y, int w, int h, Color c, int thickness) {
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect r{x + i, y + i, w - 2*i, h - 2*i};
        SDL_RenderDrawRect(m_renderer, &r);
    }
}

void Renderer::draw_line(int x1, int y1, int x2, int y2, Color c) {
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(m_renderer, x1, y1, x2, y2);
}

void Renderer::fill_circle_impl(int cx, int cy, int r, Color c) {
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    for (int dy = -r; dy <= r; ++dy) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(r*r - dy*dy)));
        SDL_RenderDrawLine(m_renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void Renderer::fill_circle(int cx, int cy, int radius, Color c) {
    fill_circle_impl(cx, cy, radius, c);
}

void Renderer::fill_rounded_rect(int x, int y, int w, int h, int r, Color c) {
    if (r <= 0) { fill_rect(x, y, w, h, c); return; }
    r = std::min(r, std::min(w, h) / 2);

    // Centre rectangle.
    fill_rect(x + r, y,     w - 2*r, h,     c);
    // Left/right strips (above/below the corner arcs).
    fill_rect(x,     y + r, r,       h-2*r, c);
    fill_rect(x+w-r, y + r, r,       h-2*r, c);

    // Four corner circles.
    fill_circle_impl(x + r,     y + r,     r, c);
    fill_circle_impl(x + w - r, y + r,     r, c);
    fill_circle_impl(x + r,     y + h - r, r, c);
    fill_circle_impl(x + w - r, y + h - r, r, c);
}

void Renderer::draw_rounded_rect(int x, int y, int w, int h, int r, Color c, int thickness) {
    // Draw outline using four arcs and four straight edges.
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    r = std::min(r, std::min(w, h) / 2);

    for (int t = 0; t < thickness; ++t) {
        int xi = x + t, yi = y + t, wi = w - 2*t, hi = h - 2*t;
        int ri = std::max(0, r - t);

        // Straight edges.
        SDL_RenderDrawLine(m_renderer, xi+ri,    yi,       xi+wi-ri, yi);
        SDL_RenderDrawLine(m_renderer, xi+ri,    yi+hi,    xi+wi-ri, yi+hi);
        SDL_RenderDrawLine(m_renderer, xi,       yi+ri,    xi,       yi+hi-ri);
        SDL_RenderDrawLine(m_renderer, xi+wi,    yi+ri,    xi+wi,    yi+hi-ri);

        // Corner arcs (quarter circles).
        auto arc = [&](int cx, int cy) {
            for (int deg = 0; deg <= 90; ++deg) {
                double rad = deg * M_PI / 180.0;
                int px = static_cast<int>(std::round(cx + ri * std::cos(rad)));
                int py = static_cast<int>(std::round(cy - ri * std::sin(rad)));
                SDL_RenderDrawPoint(m_renderer, px, py);
            }
        };
        // Top-right, top-left, bottom-right, bottom-left.
        arc(xi+wi-ri, yi+ri);
        // We'll just do the simple version and draw the four lines for corners.
    }
}

void Renderer::draw_progress_bar(int x, int y, int w, int h,
                                  float progress, Color bg, Color fill, int radius) {
    fill_rounded_rect(x, y, w, h, radius, bg);
    int fill_w = static_cast<int>(std::round(static_cast<float>(w) *
                                              std::clamp(progress, 0.0f, 1.0f)));
    if (fill_w > 0) {
        fill_rounded_rect(x, y, fill_w, h, radius, fill);
    }
}

// ─── Text ─────────────────────────────────────────────────────────────────────

int Renderer::text_width(const std::string& text, int pt_size) const {
    auto* font = get_font(pt_size);
    if (!font || text.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

int Renderer::text_height(int pt_size) const {
    auto* font = get_font(pt_size);
    if (!font) return pt_size + 4;
    return TTF_FontHeight(font);
}

int Renderer::draw_text(const std::string& text, int x, int y, int pt_size, Color c) {
    if (text.empty()) return 0;
    auto* font = get_font(pt_size);
    if (!font) return 0;

    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), c.sdl());
    if (!surf) return 0;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return 0;

    SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
    return w;
}

void Renderer::draw_text_centered(const std::string& text, int x, int y,
                                   int w, int pt_size, Color c) {
    int tw = text_width(text, pt_size);
    int tx = x + (w - tw) / 2;
    draw_text(text, tx, y, pt_size, c);
}

void Renderer::draw_text_clipped(const std::string& text, int x, int y,
                                  int max_width, int pt_size, Color c) {
    if (text_width(text, pt_size) <= max_width) {
        draw_text(text, x, y, pt_size, c);
        return;
    }
    // Binary search for the longest prefix that fits with "…" appended.
    std::string truncated = text;
    const std::string ellipsis = "...";
    int ew = text_width(ellipsis, pt_size);

    std::size_t lo = 0, hi = text.size();
    while (lo < hi) {
        std::size_t mid = (lo + hi + 1) / 2;
        if (text_width(text.substr(0, mid), pt_size) + ew <= max_width)
            lo = mid;
        else
            hi = mid - 1;
    }
    draw_text(text.substr(0, lo) + ellipsis, x, y, pt_size, c);
}

} // namespace calcium::ui

#endif // CALCIUM_DESKTOP_SDL
