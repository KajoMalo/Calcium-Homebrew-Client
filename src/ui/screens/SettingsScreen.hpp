#pragma once

#include "IScreen.hpp"
#include "../../config/Config.hpp"
#include "../widgets/Button.hpp"
#include <functional>
#include <string>
#include <vector>

namespace calcium::ui::screens {

class SettingsScreen final : public IScreen {
public:
    using SaveCallback = std::function<void(const config::AppConfig&)>;

    SettingsScreen(config::Config& config, SaveCallback save_cb);

    void on_enter() override;
    void update(float dt) override;
    void render(Renderer& r, const Theme& t) override;
#ifdef CALCIUM_DESKTOP_SDL
    bool handle_event(const SDL_Event& ev) override;
#endif

private:
    // A generic editable text field.
    struct TextField {
        std::string label;
        std::string value;
        std::string placeholder;
        bool        focused  = false;
        int x, y, w, h;

        void render(Renderer& r, const Theme& t) const;
        bool handle_click(int mx, int my);
        bool handle_key(int sym);
        bool handle_text(const char* text);
    };

    // A toggle (on/off) row.
    struct Toggle {
        std::string label;
        bool        value = false;
        int x, y, w, h;

        void render(Renderer& r, const Theme& t) const;
        bool handle_click(int mx, int my);
    };

    // A dropdown (string options).
    struct Dropdown {
        std::string              label;
        std::vector<std::string> options;
        int                      selected_idx = 0;
        bool                     open         = false;
        int x, y, w, h;

        const std::string& value() const {
            return options.empty() ? label : options[selected_idx];
        }
        void render(Renderer& r, const Theme& t) const;
        bool handle_click(int mx, int my);
    };

    void build_fields();
    void apply_to_config();
    void focused_blur_all();

    config::Config& m_config;
    SaveCallback    m_save_cb;

    // Fields.
    TextField  m_field_download_dir;
    TextField  m_field_install_dir;
    TextField  m_field_log_file;
    Dropdown   m_drop_log_level;
    Toggle     m_toggle_verify;
    TextField  m_field_timeout;
    TextField  m_field_retries;

    // Repo management.
    struct RepoRow {
        std::string id, name, url;
        bool enabled;
        widgets::Button btn_toggle;
        widgets::Button btn_remove;
        RepoRow(const config::RepositorySource& src)
            : id(src.id), name(src.name), url(src.url), enabled(src.enabled)
            , btn_toggle(src.enabled ? "Disable" : "Enable", 0, 0, 80, 28, widgets::ButtonStyle::Secondary)
            , btn_remove("Remove", 0, 0, 70, 28, widgets::ButtonStyle::Danger)
        {}
    };
    std::vector<RepoRow> m_repo_rows;

    TextField  m_field_new_repo_name;
    TextField  m_field_new_repo_url;
    widgets::Button m_btn_add_repo;
    widgets::Button m_btn_save;

    std::string m_status_message;
    int m_scroll_y = 0;
    int m_w = 1280, m_h = 720;
};

} // namespace calcium::ui::screens
