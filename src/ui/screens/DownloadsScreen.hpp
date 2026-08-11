#pragma once

#include "IScreen.hpp"
#include "../../core/AppManager.hpp"
#include "../widgets/Button.hpp"
#include "../widgets/ProgressBar.hpp"
#include <vector>
#include <string>

namespace calcium::ui::screens {

class DownloadsScreen final : public IScreen {
public:
    explicit DownloadsScreen(core::AppManager& app_manager);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    core::AppManager& m_app_manager;

    struct DownloadRow {
        std::string app_id;
        std::string name;
        std::string status_label;
        float       progress = 0.0f;
        bool        active   = false;
        std::string error;
        widgets::Button btn_cancel;

        DownloadRow(const std::string& id, const std::string& n)
            : app_id(id), name(n)
            , btn_cancel("Cancel", 0, 0, 80, 30, widgets::ButtonStyle::Ghost)
        {}
    };

    std::vector<DownloadRow> m_rows;
    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
