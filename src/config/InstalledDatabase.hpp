#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

namespace calcium::config {

/// Persisted record for a single installed application.
struct InstalledRecord {
    std::string app_id;
    std::string name;
    std::string version;
    std::string install_path;   ///< Absolute path to installed files.
    std::string content_id;     ///< PS4 Content ID (may be empty on desktop).
    std::string repo_id;        ///< Source repository ID.
    uint64_t    installed_size = 0; ///< Bytes on disk.
    std::string installed_at;   ///< ISO 8601 timestamp.
};

/// Thread-safe, JSON-backed database of installed applications.
///
/// The database is a single JSON file containing an array of InstalledRecord.
/// It is loaded once at startup and written after every mutation.
class InstalledDatabase {
public:
    InstalledDatabase() = default;

    /// Load (or create) the database at the given path.
    bool load(const std::filesystem::path& path);

    /// Persist the current state to disk.
    bool save() const;

    // ── Queries ────────────────────────────────────────────────────────────

    bool is_installed(const std::string& app_id) const;

    /// Returns the record for an installed app, or std::nullopt.
    std::optional<InstalledRecord> find(const std::string& app_id) const;

    const std::vector<InstalledRecord>& all() const { return m_records; }

    std::size_t count() const { return m_records.size(); }

    // ── Mutations ──────────────────────────────────────────────────────────

    /// Add or replace a record. Persists immediately.
    bool upsert(InstalledRecord record);

    /// Remove an installed record by app_id. Persists immediately.
    bool remove(const std::string& app_id);

    /// Remove all records. Persists immediately.
    bool clear();

    // ── Serialisation ──────────────────────────────────────────────────────
    nlohmann::json  to_json()        const;
    bool            from_json(const nlohmann::json& j);

private:
    std::vector<InstalledRecord> m_records;
    std::filesystem::path        m_path;
};

} // namespace calcium::config
