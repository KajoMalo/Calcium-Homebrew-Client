#include "InstalledDatabase.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace calcium::config {

static constexpr std::string_view TAG = "InstalledDB";

// ─── JSON helpers ─────────────────────────────────────────────────────────────

static nlohmann::json record_to_json(const InstalledRecord& r) {
    return {
        {"app_id",         r.app_id},
        {"name",           r.name},
        {"version",        r.version},
        {"install_path",   r.install_path},
        {"content_id",     r.content_id},
        {"repo_id",        r.repo_id},
        {"installed_size", r.installed_size},
        {"installed_at",   r.installed_at},
    };
}

static InstalledRecord record_from_json(const nlohmann::json& j) {
    InstalledRecord r;
    r.app_id         = j.value("app_id",         "");
    r.name           = j.value("name",           "");
    r.version        = j.value("version",        "");
    r.install_path   = j.value("install_path",   "");
    r.content_id     = j.value("content_id",     "");
    r.repo_id        = j.value("repo_id",        "");
    r.installed_size = j.value("installed_size", uint64_t{0});
    r.installed_at   = j.value("installed_at",   "");
    return r;
}

// ─── InstalledDatabase ────────────────────────────────────────────────────────

bool InstalledDatabase::load(const std::filesystem::path& path) {
    m_path = path;

    if (!std::filesystem::exists(path)) {
        logging::Logger::instance().info(TAG, "No installed database found, starting fresh.");
        return true; // empty database is valid
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open installed database: " + path.string());
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        return from_json(j);
    } catch (const nlohmann::json::parse_error& ex) {
        logging::Logger::instance().error(TAG, std::string("Malformed installed database: ") + ex.what());
        return false;
    }
}

bool InstalledDatabase::save() const {
    if (m_path.empty()) {
        logging::Logger::instance().warning(TAG, "No path set — cannot save installed database.");
        return false;
    }
    try {
        if (m_path.has_parent_path()) {
            std::filesystem::create_directories(m_path.parent_path());
        }
        std::ofstream file(m_path);
        if (!file.is_open()) {
            logging::Logger::instance().error(TAG, "Cannot write installed database: " + m_path.string());
            return false;
        }
        file << to_json().dump(4) << '\n';
        return true;
    } catch (const std::exception& ex) {
        logging::Logger::instance().error(TAG, std::string("Failed to save installed database: ") + ex.what());
        return false;
    }
}

bool InstalledDatabase::is_installed(const std::string& app_id) const {
    return std::any_of(m_records.begin(), m_records.end(),
        [&](const InstalledRecord& r) { return r.app_id == app_id; });
}

std::optional<InstalledRecord> InstalledDatabase::find(const std::string& app_id) const {
    for (const auto& r : m_records) {
        if (r.app_id == app_id) return r;
    }
    return std::nullopt;
}

bool InstalledDatabase::upsert(InstalledRecord record) {
    auto it = std::find_if(m_records.begin(), m_records.end(),
        [&](const InstalledRecord& r) { return r.app_id == record.app_id; });

    if (it != m_records.end()) {
        *it = std::move(record);
    } else {
        m_records.push_back(std::move(record));
    }
    return save();
}

bool InstalledDatabase::remove(const std::string& app_id) {
    auto before = m_records.size();
    m_records.erase(
        std::remove_if(m_records.begin(), m_records.end(),
            [&](const InstalledRecord& r) { return r.app_id == app_id; }),
        m_records.end()
    );
    if (m_records.size() == before) {
        logging::Logger::instance().warning(TAG, "remove: app_id not found: " + app_id);
        return false;
    }
    return save();
}

bool InstalledDatabase::clear() {
    m_records.clear();
    return save();
}

nlohmann::json InstalledDatabase::to_json() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : m_records) {
        arr.push_back(record_to_json(r));
    }
    return arr;
}

bool InstalledDatabase::from_json(const nlohmann::json& j) {
    if (!j.is_array()) {
        logging::Logger::instance().error(TAG, "Expected JSON array in installed database.");
        return false;
    }
    m_records.clear();
    for (const auto& item : j) {
        try {
            m_records.push_back(record_from_json(item));
        } catch (const std::exception& ex) {
            logging::Logger::instance().warning(TAG,
                std::string("Skipping malformed record: ") + ex.what());
        }
    }
    logging::Logger::instance().info(TAG,
        "Loaded " + std::to_string(m_records.size()) + " installed app(s).");
    return true;
}

} // namespace calcium::config
