#include "Config.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <stdexcept>

namespace calcium::config {

static constexpr std::string_view TAG = "Config";

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static UIConfig ui_from_json(const nlohmann::json& j) {
    UIConfig u;
    if (j.contains("theme"))          u.theme           = j.at("theme").get<std::string>();
    if (j.contains("font_size"))      u.font_size        = j.at("font_size").get<int>();
    if (j.contains("show_nsfw"))      u.show_nsfw        = j.at("show_nsfw").get<bool>();
    if (j.contains("reduced_motion")) u.reduced_motion   = j.at("reduced_motion").get<bool>();
    return u;
}

static nlohmann::json ui_to_json(const UIConfig& u) {
    return {
        {"theme",          u.theme},
        {"font_size",      u.font_size},
        {"show_nsfw",      u.show_nsfw},
        {"reduced_motion", u.reduced_motion},
    };
}

static RepositorySource repo_from_json(const nlohmann::json& j) {
    RepositorySource r;
    r.id      = j.value("id",      "");
    r.name    = j.value("name",    "");
    r.url     = j.value("url",     "");
    r.enabled = j.value("enabled", true);
    return r;
}

static nlohmann::json repo_to_json(const RepositorySource& r) {
    return {
        {"id",      r.id},
        {"name",    r.name},
        {"url",     r.url},
        {"enabled", r.enabled},
    };
}

// ─── Config ───────────────────────────────────────────────────────────────────

bool Config::from_json(const nlohmann::json& j) {
    try {
        if (j.contains("download_dir"))          m_data.download_dir         = j.at("download_dir").get<std::string>();
        if (j.contains("install_dir"))           m_data.install_dir          = j.at("install_dir").get<std::string>();
        if (j.contains("log_file"))              m_data.log_file             = j.at("log_file").get<std::string>();
        if (j.contains("log_level"))             m_data.log_level            = j.at("log_level").get<std::string>();
        if (j.contains("download_timeout_secs")) m_data.download_timeout_secs = j.at("download_timeout_secs").get<int>();
        if (j.contains("download_max_retries"))  m_data.download_max_retries  = j.at("download_max_retries").get<int>();
        if (j.contains("verify_hashes"))         m_data.verify_hashes         = j.at("verify_hashes").get<bool>();

        if (j.contains("ui"))   m_data.ui = ui_from_json(j.at("ui"));

        if (j.contains("repositories")) {
            m_data.repositories.clear();
            for (const auto& rj : j.at("repositories")) {
                m_data.repositories.push_back(repo_from_json(rj));
            }
        }

        m_loaded = true;
        return true;
    } catch (const nlohmann::json::exception& ex) {
        logging::Logger::instance().error(TAG, std::string("JSON parse error: ") + ex.what());
        return false;
    }
}

bool Config::load(const std::filesystem::path& path) {
    m_path = path;

    if (!std::filesystem::exists(path)) {
        logging::Logger::instance().info(TAG, "Config file not found, using defaults: " + path.string());
        m_loaded = true; // defaults are valid
        return true;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open config file: " + path.string());
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        return from_json(j);
    } catch (const nlohmann::json::parse_error& ex) {
        logging::Logger::instance().error(TAG, std::string("Malformed config JSON: ") + ex.what());
        return false;
    }
}

nlohmann::json Config::to_json() const {
    nlohmann::json j;
    j["download_dir"]          = m_data.download_dir;
    j["install_dir"]           = m_data.install_dir;
    j["log_file"]              = m_data.log_file;
    j["log_level"]             = m_data.log_level;
    j["download_timeout_secs"] = m_data.download_timeout_secs;
    j["download_max_retries"]  = m_data.download_max_retries;
    j["verify_hashes"]         = m_data.verify_hashes;
    j["ui"]                    = ui_to_json(m_data.ui);

    j["repositories"] = nlohmann::json::array();
    for (const auto& r : m_data.repositories) {
        j["repositories"].push_back(repo_to_json(r));
    }
    return j;
}

bool Config::save() const {
    return const_cast<Config*>(this)->save_to(m_path);
}

bool Config::save_to(const std::filesystem::path& path) {
    m_path = path;
    try {
        // Ensure the parent directory exists.
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream file(path);
        if (!file.is_open()) {
            logging::Logger::instance().error(TAG, "Cannot write config file: " + path.string());
            return false;
        }
        file << to_json().dump(4) << '\n';
        logging::Logger::instance().info(TAG, "Config saved to: " + path.string());
        return true;
    } catch (const std::exception& ex) {
        logging::Logger::instance().error(TAG, std::string("Failed to save config: ") + ex.what());
        return false;
    }
}

void Config::remove_repository(const std::string& id) {
    auto& repos = m_data.repositories;
    repos.erase(
        std::remove_if(repos.begin(), repos.end(),
            [&](const RepositorySource& r) { return r.id == id; }),
        repos.end()
    );
}

} // namespace calcium::config
