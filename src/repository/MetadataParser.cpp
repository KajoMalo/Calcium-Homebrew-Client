#include "MetadataParser.hpp"
#include "../logging/Logger.hpp"

#include <stdexcept>
#include <regex>

namespace calcium::repository {

static constexpr std::string_view TAG = "MetadataParser";

// ─── Helpers ──────────────────────────────────────────────────────────────────

/// Safely get a string value from JSON, returning a default if absent/wrong type.
static std::string jstr(const nlohmann::json& j,
                         const std::string& key,
                         const std::string& def = "") {
    if (j.contains(key) && j.at(key).is_string()) {
        return j.at(key).get<std::string>();
    }
    return def;
}

/// Safely get a uint64 value.
static uint64_t ju64(const nlohmann::json& j,
                      const std::string& key,
                      uint64_t def = 0) {
    if (j.contains(key) && j.at(key).is_number_unsigned()) {
        return j.at(key).get<uint64_t>();
    }
    return def;
}

/// Safely get a string array.
static std::vector<std::string> jstr_array(const nlohmann::json& j,
                                            const std::string& key) {
    std::vector<std::string> result;
    if (!j.contains(key) || !j.at(key).is_array()) return result;
    for (const auto& elem : j.at(key)) {
        if (elem.is_string()) result.push_back(elem.get<std::string>());
    }
    return result;
}

// ─── CompatInfo ───────────────────────────────────────────────────────────────

CompatInfo MetadataParser::parse_compat(const nlohmann::json& j) {
    CompatInfo info;
    info.status           = parse_compat_status(jstr(j, "status", "unknown"));
    info.tested_firmware  = jstr_array(j, "tested_firmware");
    info.notes            = jstr(j, "notes");
    return info;
}

// ─── Single app ───────────────────────────────────────────────────────────────

ParseResult<AppMetadata> MetadataParser::parse_app(const nlohmann::json& j) {
    try {
        AppMetadata m;

        m.id               = jstr(j, "id");
        m.name             = jstr(j, "name");
        m.version          = jstr(j, "version");
        m.content_id       = jstr(j, "content_id");
        m.author           = jstr(j, "author");
        m.license          = jstr(j, "license");
        m.category         = jstr(j, "category");
        m.tags             = jstr_array(j, "tags");
        m.description      = jstr(j, "description");
        m.long_description = jstr(j, "long_description");
        m.changelog        = jstr(j, "changelog");
        m.icon_url         = jstr(j, "icon_url");
        m.screenshot_urls  = jstr_array(j, "screenshots");
        m.download_url     = jstr(j, "download_url");
        m.download_size    = ju64(j, "download_size");
        m.installed_size   = ju64(j, "installed_size");
        m.sha256           = jstr(j, "sha256");
        m.min_firmware     = jstr(j, "min_firmware");
        m.updated_at       = jstr(j, "updated_at");

        if (j.contains("compatibility") && j.at("compatibility").is_object()) {
            m.compatibility = parse_compat(j.at("compatibility"));
        }

        auto err = validate(m);
        if (!err.empty()) {
            return ParseResult<AppMetadata>::failure(
                "Validation failed for '" + m.id + "': " + err);
        }

        return ParseResult<AppMetadata>::success(std::move(m));

    } catch (const nlohmann::json::exception& ex) {
        return ParseResult<AppMetadata>::failure(
            std::string("JSON error parsing app: ") + ex.what());
    }
}

// ─── Index ────────────────────────────────────────────────────────────────────

ParseResult<std::vector<AppMetadata>>
MetadataParser::parse_index(const std::string& json_text) {
    if (json_text.empty()) {
        return ParseResult<std::vector<AppMetadata>>::failure("Empty JSON input");
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json_text);
    } catch (const nlohmann::json::parse_error& ex) {
        return ParseResult<std::vector<AppMetadata>>::failure(
            std::string("Malformed JSON: ") + ex.what());
    }

    // Validate schema_version.
    if (root.contains("schema_version")) {
        auto sv = root.at("schema_version").get<std::string>();
        if (sv != "1.0") {
            logging::Logger::instance().warning(TAG,
                "Unknown schema_version: " + sv + ". Attempting to parse anyway.");
        }
    }

    if (!root.contains("apps") || !root.at("apps").is_array()) {
        return ParseResult<std::vector<AppMetadata>>::failure(
            "Index JSON missing 'apps' array");
    }

    std::vector<AppMetadata> apps;
    int skipped = 0;

    for (const auto& entry : root.at("apps")) {
        auto result = parse_app(entry);
        if (result.ok()) {
            apps.push_back(std::move(*result.value));
        } else {
            logging::Logger::instance().warning(TAG,
                "Skipping app entry: " + result.error);
            ++skipped;
        }
    }

    if (skipped > 0) {
        logging::Logger::instance().warning(TAG,
            "Skipped " + std::to_string(skipped) + " invalid app entries.");
    }

    logging::Logger::instance().info(TAG,
        "Parsed " + std::to_string(apps.size()) + " app(s) from index.");

    return ParseResult<std::vector<AppMetadata>>::success(std::move(apps));
}

// ─── Validation ───────────────────────────────────────────────────────────────

std::string MetadataParser::validate(const AppMetadata& m) {
    if (m.id.empty())           return "Missing required field: id";
    if (m.name.empty())         return "Missing required field: name";
    if (m.version.empty())      return "Missing required field: version";
    if (m.download_url.empty()) return "Missing required field: download_url";
    if (m.author.empty())       return "Missing required field: author";

    // id must look like a reverse-DNS identifier or a simple slug.
    static const std::regex id_re{R"([a-zA-Z0-9][a-zA-Z0-9._\-]{1,127})"};
    if (!std::regex_match(m.id, id_re)) {
        return "Invalid id format: '" + m.id + "'";
    }

    // download_url must be http or https.
    if (m.download_url.substr(0, 4) != "http") {
        return "download_url must start with http(s): " + m.download_url;
    }

    // sha256, if present, must be 64 hex chars.
    if (!m.sha256.empty()) {
        static const std::regex sha_re{R"([0-9a-fA-F]{64})"};
        if (!std::regex_match(m.sha256, sha_re)) {
            return "Invalid sha256 value for app '" + m.id + "'";
        }
    }

    return {}; // all good
}

} // namespace calcium::repository
