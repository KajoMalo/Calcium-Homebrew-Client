#include "Filesystem.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <system_error>

namespace calcium::filesystem {

static constexpr std::string_view TAG = "Filesystem";

namespace fs = std::filesystem;

// ─── Queries ──────────────────────────────────────────────────────────────────

bool Filesystem::exists(const fs::path& path) const {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool Filesystem::is_directory(const fs::path& path) const {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool Filesystem::is_regular_file(const fs::path& path) const {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

uint64_t Filesystem::file_size(const fs::path& path) const {
    std::error_code ec;
    auto sz = fs::file_size(path, ec);
    if (ec) {
        logging::Logger::instance().warning(TAG,
            "file_size failed for " + path.string() + ": " + ec.message());
        return 0;
    }
    return static_cast<uint64_t>(sz);
}

SpaceInfo Filesystem::space(const fs::path& path) const {
    std::error_code ec;
    // Use the path itself if it exists, otherwise its parent.
    fs::path query = exists(path) ? path : path.parent_path();
    auto si = fs::space(query, ec);
    if (ec) {
        logging::Logger::instance().warning(TAG,
            "space query failed for " + query.string() + ": " + ec.message());
        return {};
    }
    return SpaceInfo{
        static_cast<uint64_t>(si.available),
        static_cast<uint64_t>(si.capacity),
        static_cast<uint64_t>(si.capacity - si.free),
    };
}

std::vector<fs::path> Filesystem::list_directory(const fs::path& path) const {
    std::vector<fs::path> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path, ec)) {
        result.push_back(entry.path());
    }
    if (ec) {
        logging::Logger::instance().warning(TAG,
            "list_directory failed for " + path.string() + ": " + ec.message());
    }
    return result;
}

// ─── Mutations ────────────────────────────────────────────────────────────────

bool Filesystem::create_directories(const fs::path& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec) {
        logging::Logger::instance().error(TAG,
            "create_directories failed for " + path.string() + ": " + ec.message());
        return false;
    }
    return true;
}

bool Filesystem::remove(const fs::path& path) {
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        logging::Logger::instance().error(TAG,
            "remove failed for " + path.string() + ": " + ec.message());
        return false;
    }
    return true;
}

bool Filesystem::remove_all(const fs::path& path) {
    std::error_code ec;
    fs::remove_all(path, ec);
    if (ec) {
        logging::Logger::instance().error(TAG,
            "remove_all failed for " + path.string() + ": " + ec.message());
        return false;
    }
    return true;
}

bool Filesystem::rename(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) {
        logging::Logger::instance().error(TAG,
            "rename failed " + from.string() + " -> " + to.string() + ": " + ec.message());
        return false;
    }
    return true;
}

bool Filesystem::copy_file(const fs::path& from, const fs::path& to, bool overwrite) {
    std::error_code ec;
    auto opts = overwrite
        ? fs::copy_options::overwrite_existing
        : fs::copy_options::none;
    fs::copy_file(from, to, opts, ec);
    if (ec) {
        logging::Logger::instance().error(TAG,
            "copy_file failed " + from.string() + " -> " + to.string() + ": " + ec.message());
        return false;
    }
    return true;
}

// ─── I/O helpers ──────────────────────────────────────────────────────────────

std::optional<std::vector<uint8_t>> Filesystem::read_bytes(const fs::path& path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open for reading: " + path.string());
        return std::nullopt;
    }
    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool Filesystem::write_bytes(const fs::path& path, const std::vector<uint8_t>& data) {
    // Ensure parent directory exists.
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open for writing: " + path.string());
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::optional<std::string> Filesystem::read_text(const fs::path& path) const {
    std::ifstream file(path);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open for reading: " + path.string());
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool Filesystem::write_text(const fs::path& path, const std::string& text) {
    if (path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open for writing: " + path.string());
        return false;
    }
    file << text;
    return file.good();
}

// ─── Path helpers ─────────────────────────────────────────────────────────────

fs::path Filesystem::temp_directory() const {
    std::error_code ec;
    auto p = fs::temp_directory_path(ec);
    if (ec) return fs::path{"./tmp"};
    return p / "calcium-client";
}

fs::path Filesystem::app_data_directory() const {
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return fs::path{appdata} / "CalciumClient";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) return fs::path{home} / "Library" / "Application Support" / "CalciumClient";
#else
    const char* home = std::getenv("HOME");
    if (home) return fs::path{home} / ".config" / "calcium-client";
#endif
    return fs::path{"./data"};
}

} // namespace calcium::filesystem
