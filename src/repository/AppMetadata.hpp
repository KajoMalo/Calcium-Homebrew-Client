#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace calcium::repository {

/// Compatibility status for a particular firmware version.
enum class CompatStatus {
    Unknown,
    Verified,    ///< Tested and confirmed working.
    Partial,     ///< Works with known limitations.
    Broken,      ///< Known not to work.
};

inline constexpr std::string_view to_string(CompatStatus s) {
    switch (s) {
        case CompatStatus::Unknown:  return "unknown";
        case CompatStatus::Verified: return "verified";
        case CompatStatus::Partial:  return "partial";
        case CompatStatus::Broken:   return "broken";
    }
    return "unknown";
}

inline CompatStatus parse_compat_status(const std::string& s) {
    if (s == "verified") return CompatStatus::Verified;
    if (s == "partial")  return CompatStatus::Partial;
    if (s == "broken")   return CompatStatus::Broken;
    return CompatStatus::Unknown;
}

/// Compatibility block embedded in AppMetadata.
struct CompatInfo {
    CompatStatus        status;
    std::vector<std::string> tested_firmware; ///< e.g. ["9.00", "11.00"]
    std::string         notes;
};

/// All metadata fields for a single homebrew application.
///
/// Every field has a safe default so partial metadata does not crash the UI.
struct AppMetadata {
    // ── Identity ──────────────────────────────────────────────────────────
    std::string id;             ///< Reverse-DNS style, e.g. "com.example.retroarch"
    std::string name;
    std::string version;
    std::string content_id;     ///< PS4 Content ID (optional, may be empty)

    // ── Attribution ───────────────────────────────────────────────────────
    std::string author;
    std::string license;

    // ── Classification ────────────────────────────────────────────────────
    std::string category;       ///< e.g. "emulator", "utility", "game", "media"
    std::vector<std::string> tags;

    // ── Description ───────────────────────────────────────────────────────
    std::string description;        ///< Short one-line summary.
    std::string long_description;   ///< Full markdown-safe description.
    std::string changelog;          ///< Latest-version changelog text.

    // ── Media ─────────────────────────────────────────────────────────────
    std::string icon_url;
    std::vector<std::string> screenshot_urls;

    // ── Download ──────────────────────────────────────────────────────────
    std::string download_url;
    uint64_t    download_size   = 0;    ///< Bytes.
    uint64_t    installed_size  = 0;    ///< Estimated bytes after install.
    std::string sha256;                 ///< Hex-encoded SHA-256 of the package.

    // ── Firmware requirement ──────────────────────────────────────────────
    std::string min_firmware;   ///< Minimum required firmware, e.g. "9.00"

    // ── Compatibility ─────────────────────────────────────────────────────
    CompatInfo  compatibility;

    // ── Timestamps ────────────────────────────────────────────────────────
    std::string updated_at;     ///< ISO 8601

    // ── Source tracking (filled by RepositoryManager, not from JSON) ──────
    std::string repo_id;        ///< Which repository supplied this record.

    // ── Install state (filled by AppManager at runtime) ───────────────────
    bool        is_installed     = false;
    std::string installed_version;

    bool has_update() const {
        return is_installed && !installed_version.empty() && installed_version != version;
    }
};

/// Lightweight index entry — used for list views before loading full metadata.
struct AppSummary {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string category;
    std::string description;
    std::string icon_url;
    bool        is_installed = false;
};

} // namespace calcium::repository
