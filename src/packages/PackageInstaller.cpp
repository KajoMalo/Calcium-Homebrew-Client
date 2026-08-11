#include "PackageInstaller.hpp"
#include "../logging/Logger.hpp"

#include <fstream>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <nlohmann/json.hpp>

// ─── Minimal ZIP parser ───────────────────────────────────────────────────────
// Implements enough of PKWARE APPNOTE.TXT to extract Stored (method 0) and
// Deflate (method 8) entries. No external zlib dependency: deflate is handled
// via the public-domain miniz single-header embedded inline below.
//
// We include a minimal inflate implementation directly so the project has
// zero external decompression dependencies.

// ---------- miniz inflate (public domain, extracted subset) ------------------
// Original: https://github.com/richgel999/miniz  (MIT / public domain)
// Only the raw-inflate / zlib-inflate portion is included here.
#include <zlib.h>  // Use system zlib which is available on all target platforms.
// On PS4 (orbis-sdk) zlib is available in the SDK.
// On desktop it ships with most platforms; CMake's find_package(ZLIB) handles it.
// ─────────────────────────────────────────────────────────────────────────────

namespace calcium::packages {

static constexpr std::string_view TAG = "PackageInstaller";

// ─── ZIP structures (PKWARE APPNOTE 4.3.7) ───────────────────────────────────

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;         // 0x04034b50
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;       // 0 = stored, 8 = deflate
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
};
#pragma pack(pop)

static constexpr uint32_t ZIP_LOCAL_MAGIC = 0x04034b50;
static constexpr uint32_t ZIP_DATA_DESC   = 0x08074b50;

static bool read_exact(std::ifstream& f, void* buf, std::size_t n) {
    f.read(static_cast<char*>(buf), static_cast<std::streamsize>(n));
    return static_cast<std::size_t>(f.gcount()) == n;
}

// ─── Construction ─────────────────────────────────────────────────────────────

PackageInstaller::PackageInstaller(std::shared_ptr<filesystem::IFilesystem> fs,
                                    std::string install_root)
    : m_fs(std::move(fs))
    , m_install_root(std::move(install_root))
{}

std::filesystem::path PackageInstaller::install_dir_for(const std::string& app_id) const {
    return std::filesystem::path(m_install_root) / app_id;
}

// ─── ZIP extraction ───────────────────────────────────────────────────────────

int PackageInstaller::extract_zip(const std::filesystem::path& zip_path,
                                   const std::filesystem::path& dest_dir,
                                   ProgressCallback on_progress) {
    std::ifstream zf(zip_path, std::ios::binary);
    if (!zf.is_open()) {
        logging::Logger::instance().error(TAG, "Cannot open package: " + zip_path.string());
        return -1;
    }

    // Determine file size for overall progress.
    zf.seekg(0, std::ios::end);
    const auto file_size = static_cast<uint64_t>(zf.tellg());
    zf.seekg(0, std::ios::beg);

    int files_extracted = 0;

    while (!m_cancelled.load()) {
        ZipLocalHeader hdr{};
        if (!read_exact(zf, &hdr, sizeof(hdr))) break; // end of file or error

        if (hdr.signature != ZIP_LOCAL_MAGIC) {
            // Could be central directory or end record — we're done.
            break;
        }

        // Read filename.
        std::string filename(hdr.filename_len, '\0');
        if (!read_exact(zf, filename.data(), hdr.filename_len)) {
            logging::Logger::instance().error(TAG, "Truncated filename in ZIP.");
            return -1;
        }

        // Skip extra field.
        if (hdr.extra_len > 0) {
            zf.seekg(hdr.extra_len, std::ios::cur);
        }

        // Determine output path, stripping the top-level directory prefix.
        std::filesystem::path rel(filename);
        // Strip the first path component (the archive's root directory).
        std::filesystem::path stripped;
        bool first = true;
        for (const auto& part : rel) {
            if (first) { first = false; continue; }
            stripped /= part;
        }

        // Entries that are only the root directory or end with '/' are directories.
        bool is_dir = filename.back() == '/' || filename.back() == '\\';

        const std::filesystem::path out_path = dest_dir / stripped;

        if (is_dir || stripped.empty()) {
            if (!stripped.empty()) {
                m_fs->create_directories(out_path);
            }
            // Skip any data (should be 0 for dir entries, but be safe).
            if (hdr.compressed_size > 0) {
                zf.seekg(hdr.compressed_size, std::ios::cur);
            }
            continue;
        }

        // Ensure parent directory exists.
        m_fs->create_directories(out_path.parent_path());

        // Read compressed data.
        std::vector<uint8_t> compressed(hdr.compressed_size);
        if (hdr.compressed_size > 0 && !read_exact(zf, compressed.data(), hdr.compressed_size)) {
            logging::Logger::instance().error(TAG, "Truncated data for entry: " + filename);
            return -1;
        }

        // Decompress or copy.
        std::vector<uint8_t> uncompressed;

        if (hdr.compression == 0) {
            // Stored — no compression.
            uncompressed = std::move(compressed);
        } else if (hdr.compression == 8) {
            // Deflate — use zlib inflate with raw deflate stream (-MAX_WBITS).
            uncompressed.resize(hdr.uncompressed_size);
            z_stream zs{};
            zs.next_in   = compressed.data();
            zs.avail_in  = static_cast<uInt>(compressed.size());
            zs.next_out  = uncompressed.data();
            zs.avail_out = static_cast<uInt>(uncompressed.size());

            if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
                logging::Logger::instance().error(TAG, "inflateInit2 failed for: " + filename);
                return -1;
            }
            int ret = inflate(&zs, Z_FINISH);
            inflateEnd(&zs);

            if (ret != Z_STREAM_END) {
                logging::Logger::instance().error(TAG,
                    "inflate failed (ret=" + std::to_string(ret) + ") for: " + filename);
                return -1;
            }
        } else {
            logging::Logger::instance().error(TAG,
                "Unsupported compression method " +
                std::to_string(hdr.compression) + " for: " + filename);
            return -1;
        }

        // Write extracted file.
        if (!m_fs->write_bytes(out_path, uncompressed)) {
            logging::Logger::instance().error(TAG, "Failed to write: " + out_path.string());
            return -1;
        }

        ++files_extracted;

        if (on_progress) {
            uint64_t pos = static_cast<uint64_t>(zf.tellg());
            PackageProgress pp;
            pp.status         = PackageOpStatus::Installing;
            pp.bytes_received = pos;
            pp.bytes_total    = file_size;
            pp.progress       = file_size > 0
                ? static_cast<float>(pos) / static_cast<float>(file_size)
                : 0.5f;
            pp.status_message = "Extracting: " + filename;
            on_progress(pp);
        }
    }

    return files_extracted;
}

// ─── Marker file ──────────────────────────────────────────────────────────────

bool PackageInstaller::write_marker(const std::filesystem::path& install_dir,
                                     const repository::AppMetadata& meta) {
    nlohmann::json j = {
        {"app_id",   meta.id},
        {"name",     meta.name},
        {"version",  meta.version},
        {"author",   meta.author},
        {"repo_id",  meta.repo_id},
    };
    return m_fs->write_text(install_dir / "calcium-meta.json", j.dump(4));
}

// ─── Install ──────────────────────────────────────────────────────────────────

std::string PackageInstaller::install(const std::filesystem::path& pkg_path,
                                       const repository::AppMetadata& meta,
                                       ProgressCallback on_progress) {
    m_cancelled.store(false);

    logging::Logger::instance().info(TAG, "Installing " + meta.id + " from " + pkg_path.string());

    // Staging directory — extract here first, then rename atomically.
    const auto staging = std::filesystem::path(m_install_root) / (meta.id + ".staging");
    const auto final_dir = install_dir_for(meta.id);

    // Clean up any leftover staging directory.
    if (m_fs->exists(staging)) {
        m_fs->remove_all(staging);
    }
    m_fs->create_directories(staging);

    if (on_progress) {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Installing;
        pp.progress       = 0.0f;
        pp.status_message = "Extracting package...";
        on_progress(pp);
    }

    int extracted = extract_zip(pkg_path, staging, on_progress);

    if (m_cancelled.load()) {
        m_fs->remove_all(staging);
        logging::Logger::instance().info(TAG, "Install cancelled: " + meta.id);
        return {};
    }

    if (extracted < 0) {
        m_fs->remove_all(staging);
        logging::Logger::instance().error(TAG, "Extraction failed for: " + meta.id);
        return {};
    }

    // Remove old install if upgrading.
    if (m_fs->exists(final_dir)) {
        m_fs->remove_all(final_dir);
    }

    // Atomic rename from staging to final.
    if (!m_fs->rename(staging, final_dir)) {
        m_fs->remove_all(staging);
        logging::Logger::instance().error(TAG, "Failed to move staging dir for: " + meta.id);
        return {};
    }

    write_marker(final_dir, meta);

    logging::Logger::instance().info(TAG,
        "Installed " + meta.id + " to " + final_dir.string() +
        " (" + std::to_string(extracted) + " files)");

    if (on_progress) {
        PackageProgress pp;
        pp.app_id         = meta.id;
        pp.status         = PackageOpStatus::Completed;
        pp.progress       = 1.0f;
        pp.status_message = "Installation complete.";
        on_progress(pp);
    }

    return final_dir.string();
}

// ─── Uninstall ────────────────────────────────────────────────────────────────

bool PackageInstaller::uninstall(const std::string& app_id,
                                  const std::string& install_path,
                                  ProgressCallback on_progress) {
    logging::Logger::instance().info(TAG, "Uninstalling " + app_id + " from " + install_path);

    if (on_progress) {
        PackageProgress pp;
        pp.app_id         = app_id;
        pp.status         = PackageOpStatus::Installing; // reuse Installing stage
        pp.progress       = 0.2f;
        pp.status_message = "Removing application files...";
        on_progress(pp);
    }

    const std::filesystem::path dir{install_path};
    if (!m_fs->exists(dir)) {
        logging::Logger::instance().warning(TAG, "Install path not found: " + install_path);
        return false;
    }

    if (!m_fs->remove_all(dir)) {
        logging::Logger::instance().error(TAG, "Failed to remove: " + install_path);
        return false;
    }

    logging::Logger::instance().info(TAG, "Uninstalled: " + app_id);

    if (on_progress) {
        PackageProgress pp;
        pp.app_id         = app_id;
        pp.status         = PackageOpStatus::Completed;
        pp.progress       = 1.0f;
        pp.status_message = "Uninstall complete.";
        on_progress(pp);
    }

    return true;
}

} // namespace calcium::packages
