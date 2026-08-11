#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace calcium::config {

/// UI display preferences.
struct UIConfig {
    std::string theme        = "dark";
    int         font_size    = 18;
    bool        show_nsfw    = false;
    bool        reduced_motion = false;
};

/// A single configured repository source.
struct RepositorySource {
    std::string id;
    std::string name;
    std::string url;
    bool        enabled = true;
};

/// Top-level application configuration.
/// Loaded from and persisted to a JSON file on disk.
struct AppConfig {
    // Paths
    std::string download_dir    = "./downloads";
    std::string install_dir     = "./installed";
    std::string log_file        = "./calcium.log";

    // Logging
    std::string log_level       = "info";

    // Repositories
    std::vector<RepositorySource> repositories;

    // UI
    UIConfig ui;

    // Network
    int  download_timeout_secs  = 30;
    int  download_max_retries   = 3;
    bool verify_hashes          = true;
};

/// Manages loading, saving, and accessing application configuration.
class Config {
public:
    Config() = default;

    /// Load configuration from a JSON file.
    /// Returns false and logs an error if the file cannot be parsed.
    bool load(const std::filesystem::path& path);

    /// Save configuration to the path it was last loaded from.
    bool save() const;

    /// Save to an explicit path (also updates the stored path).
    bool save_to(const std::filesystem::path& path);

    /// Populate from a JSON object (used in tests).
    bool from_json(const nlohmann::json& j);

    /// Serialise to a JSON object.
    nlohmann::json to_json() const;

    // ── Accessors ──────────────────────────────────────────────────────────
    const AppConfig& data() const { return m_data; }
    AppConfig&       data()       { return m_data; }

    const std::filesystem::path& path() const { return m_path; }

    /// Returns true if a config file has been successfully loaded.
    bool is_loaded() const { return m_loaded; }

    /// Convenience accessors for frequently used settings.
    const std::string& download_dir()  const { return m_data.download_dir; }
    const std::string& install_dir()   const { return m_data.install_dir; }
    const std::string& log_level()     const { return m_data.log_level; }
    bool               verify_hashes() const { return m_data.verify_hashes; }

    const std::vector<RepositorySource>& repositories() const {
        return m_data.repositories;
    }

    void add_repository(RepositorySource src) {
        m_data.repositories.push_back(std::move(src));
    }

    void remove_repository(const std::string& id);

private:
    AppConfig             m_data;
    std::filesystem::path m_path;
    bool                  m_loaded = false;
};

} // namespace calcium::config
