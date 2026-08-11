#pragma once

#include "IFilesystem.hpp"

namespace calcium::filesystem {

/// std::filesystem-backed implementation of IFilesystem.
/// Used on desktop and as the default implementation everywhere C++17 is available.
class Filesystem final : public IFilesystem {
public:
    Filesystem() = default;

    bool     exists(const std::filesystem::path& path) const override;
    bool     is_directory(const std::filesystem::path& path) const override;
    bool     is_regular_file(const std::filesystem::path& path) const override;
    uint64_t file_size(const std::filesystem::path& path) const override;
    SpaceInfo space(const std::filesystem::path& path) const override;

    std::vector<std::filesystem::path>
        list_directory(const std::filesystem::path& path) const override;

    bool create_directories(const std::filesystem::path& path) override;
    bool remove(const std::filesystem::path& path) override;
    bool remove_all(const std::filesystem::path& path) override;
    bool rename(const std::filesystem::path& from,
                const std::filesystem::path& to) override;
    bool copy_file(const std::filesystem::path& from,
                   const std::filesystem::path& to,
                   bool overwrite = false) override;

    std::optional<std::vector<uint8_t>>
        read_bytes(const std::filesystem::path& path) const override;

    bool write_bytes(const std::filesystem::path& path,
                     const std::vector<uint8_t>& data) override;

    std::optional<std::string>
        read_text(const std::filesystem::path& path) const override;

    bool write_text(const std::filesystem::path& path,
                    const std::string& text) override;

    std::filesystem::path temp_directory() const override;
    std::filesystem::path app_data_directory() const override;
};

} // namespace calcium::filesystem
