#include "DownloadsScreen.hpp"

#ifdef CALCIUM_DESKTOP_SDL
#include <SDL2/SDL.h>
#endif

namespace calcium::ui::screens {

DownloadsScreen::DownloadsScreen(core::AppManager& app_manager)
    : m_app_manager(app_manager)
{}

void DownloadsScreen::on_enter() {
    m_rows.clear();
    m_app_manager.set_event_callback([this](const core::AppEvent& ev) {
        using T = core::AppEvent::Type;
        auto it = std::find_if(m_rows.begin(), m_rows.end(),
            [&](const DownloadRow& r) { return r.app_id == ev.app_id; });

        if (ev.type == T::InstallStarted) {
            if (it == m_rows.end()) {
                auto meta = m_app_manager.find_app(ev.app_id);
                std::string name = meta ? meta->name : ev.app_id;
                m_rows.emplace_back(ev.app_id, name);
                it = m_rows.end() - 1;
            }
            it->status_label = "Downloading...";
            it->active       = true;
            it->progress     = 0.0f;
        } else if (ev.type == T::InstallProgress) {
            if (it != m_rows.end()) {
                it->progress     = ev.progress;
                it->status_label = ev.message;
                it->active       = true;
            }
        } else if (ev.type == T::InstallComplete) {
            if (it != m_rows.end()) {
                it->progress     = 1.0f;
                it->status_label = "Complete";
                it->active       = false;
            }
        } else if (ev.type == T::InstallFailed) {
            if (it != m_rows.end()) {
                it->status_label = "Failed";
                it->error        = ev.message;
                it->active       = false;
            }
        }
    });
}

void DownloadsScreen::update(float /*dt*/) {
    // Position cancel buttons.
    int row_h = 72;
    int start_y = 80;
    for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
        int ry = start_y + i * row_h;
        m_rows[i].btn_cancel.set_position(m_w - 100, ry + (row_h - 30) / 2);
        m_rows[i].btn_cancel.set_enabled(m_rows[i].active);
    }
}

void DownloadsScreen::render(Renderer& r, const Theme& t) {
    r.fill_rect(0, 0, m_w, m_h, t.bg_primary);
    r.fill_rect(0, 0, m_w, 64, t.bg_secondary);
    r.draw_text("Downloads", t.spacing.lg, 18, t.font_size_large, t.text_primary);
    r.draw_line(0, 64, m_w, 64, t.border);

    if (m_rows.empty()) {
        r.draw_text_centered("No active or recent downloads.",
                             0, m_h / 2, m_w, t.font_size_body, t.text_secondary);
        return;
    }

    int row_h = 72;
    int ry    = 80;
    for (const auto& row : m_rows) {
        // Row background.
        r.fill_rect(0, ry, m_w, row_h, t.bg_secondary);
        r.draw_line(0, ry + row_h - 1, m_w, ry + row_h - 1, t.border);

        // App name.
        r.draw_text_clipped(row.name, t.spacing.lg, ry + t.spacing.sm,
                            m_w / 3, t.font_size_body, t.text_primary);

        // Status label.
        r.draw_text_clipped(row.status_label,
                            t.spacing.lg, ry + t.spacing.sm + 22,
                            m_w / 3, t.font_size_small, t.text_secondary);

        // Progress bar.
        int pb_x = m_w / 3 + t.spacing.lg;
        int pb_w = m_w - pb_x - 120;
        int pb_y = ry + (row_h - 12) / 2;

        if (row.active) {
            r.draw_progress_bar(pb_x, pb_y, pb_w, 12,
                                row.progress, t.progress_bg, t.progress_fill, 6);
            // Percentage label.
            std::string pct = std::to_string(static_cast<int>(row.progress * 100)) + "%";
            r.draw_text(pct, pb_x + pb_w + t.spacing.sm, pb_y,
                        t.font_size_small, t.text_secondary);
        } else if (!row.error.empty()) {
            r.draw_text_clipped(row.error, pb_x, pb_y, pb_w,
                                t.font_size_small, t.error_color);
        } else {
            // Complete — full bar.
            r.draw_progress_bar(pb_x, pb_y, pb_w, 12,
                                1.0f, t.progress_bg, t.success, 6);
        }

        row.btn_cancel.render(r, t);
        ry += row_h;
    }
}

#ifdef CALCIUM_DESKTOP_SDL
bool DownloadsScreen::handle_event(const SDL_Event& ev) {
    if (ev.type == SDL_WINDOWEVENT &&
        ev.window.event == SDL_WINDOWEVENT_RESIZED) {
        m_w = ev.window.data1;
        m_h = ev.window.data2;
        return false;
    }
    if (ev.type == SDL_MOUSEMOTION) {
        for (auto& row : m_rows)
            row.btn_cancel.handle_mouse_move(ev.motion.x, ev.motion.y);
        return false;
    }
    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
        for (auto& row : m_rows) {
            if (row.btn_cancel.handle_mouse_click(ev.button.x, ev.button.y)) {
                m_app_manager.cancel();
                row.status_label = "Cancelling...";
                return true;
            }
        }
    }
    return false;
}
#endif

} // namespace calcium::ui::screens
