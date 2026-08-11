#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <filesystem>

namespace calcium::filesystem {

/// Result of a storage-space query.
struct SpaceInfo {
    uint64_t available = 0;  ///< Bytes available to the application.
    uint64_t total     = 0;  ///< Total capacity of the volume.
    uint64_t used      = 0;  ///< Bytes currently in use.
};

/// Abstract filesystem interface.
///
/// All path-related operations go through this interface so platform-specific
/// behaviour (PS4 vs. desktop) can be swapped without touching callers.
class IFilesystem {
public:
    virtual ~IFilesystem() = default;

    // ── Queries ────────────────────────────────────────────────────────────
    virtual bool     exists(const std::filesystem::path& path) const = 0;
    virtual bool     is_directory(const std::filesystem::path& path) const = 0;
    virtual bool     is_regular_file(const std::filesystem::path& path) const = 0;
    virtual uint64_t file_size(const std::filesystem::path& path) const = 0;
    virtual SpaceInfo space(const std::filesystem::path& path) const = 0;

    /// List immediate children of a directory.
    virtual std::vector<std::filesystem::path>
        list_directory(const std::filesystem::path& path) const = 0;

    // ── Mutations ──────────────────────────────────────────────────────────
    virtual bool create_directories(const std::filesystem::path& path) = 0;
    virtual bool remove(const std::filesystem::path& path) = 0;
    virtual bool remove_all(const std::filesystem::path& path) = 0;
    virtual bool rename(const std::filesystem::path& from,
                        const std::filesystem::path& to) = 0;
    virtual bool copy_file(const std::filesystem::path& from,
                           const std::filesystem::path& to,
                           bool overwrite = false) = 0;

    // ── I/O helpers ────────────────────────────────────────────────────────
    virtual std::optional<std::vector<uint8_t>>
        read_bytes(const std::filesystem::path& path) const = 0;

    virtual bool write_bytes(const std::filesystem::path& path,
                             const std::vector<uint8_t>& data) = 0;

    virtual std::optional<std::string>
        read_text(const std::filesystem::path& path) const = 0;

    virtual bool write_text(const std::filesystem::path& path,
                            const std::string& text) = 0;

    // ── Path helpers (may return platform-specific roots) ─────────────────
    virtual std::filesystem::path temp_directory() const = 0;
    virtual std::filesystem::path app_data_directory() const = 0;
};

} // namespace calcium::filesystem
