#pragma once

#include "../repository/AppMetadata.hpp"
#include "../filesystem/IFilesystem.hpp"
#include "IPackageManager.hpp"

#include <filesystem>
#include <string>
#include <atomic>

namespace calcium::packages {

/// Installs and removes packages on the local filesystem.
///
/// A "package" in Calcium is a ZIP archive whose top-level directory becomes
/// the application install directory. The installer:
///   1. Validates the archive structure.
///   2. Extracts to a staging directory.
///   3. Moves the staged directory to the final install location atomically.
///   4. Writes a "calcium-meta.json" marker so the install can be located later.
///
/// The ZIP extraction is implemented without external dependencies using the
/// standard DEFLATE/Stored entry logic from the ZIP spec (PKWARE APPNOTE).
class PackageInstaller {
public:
    explicit PackageInstaller(std::shared_ptr<filesystem::IFilesystem> fs,
                               std::string install_root);

    /// Install a downloaded package archive.
    /// @param pkg_path   Path to the downloaded .zip / .pkg file.
    /// @param meta       Full app metadata (used for directory naming and marker).
    /// @param on_progress  Progress callback (optional).
    /// @returns          Absolute path to the install directory on success,
    ///                   or empty string on failure.
    std::string install(const std::filesystem::path& pkg_path,
                        const repository::AppMetadata& meta,
                        ProgressCallback on_progress = nullptr);

    /// Remove an installed application by deleting its directory.
    bool uninstall(const std::string& app_id,
                   const std::string& install_path,
                   ProgressCallback on_progress = nullptr);

    /// Request cancellation of the current operation.
    void cancel() { m_cancelled.store(true); }
    bool is_cancelled() const { return m_cancelled.load(); }

    /// Derive the expected install directory for an app.
    std::filesystem::path install_dir_for(const std::string& app_id) const;

private:
    /// Extract a ZIP archive to dest_dir. Returns number of files extracted,
    /// or -1 on error.
    int extract_zip(const std::filesystem::path& zip_path,
                    const std::filesystem::path& dest_dir,
                    ProgressCallback on_progress);

    /// Write the calcium-meta.json marker file.
    bool write_marker(const std::filesystem::path& install_dir,
                      const repository::AppMetadata& meta);

    std::shared_ptr<filesystem::IFilesystem> m_fs;
    std::string                              m_install_root;
    std::atomic<bool>                        m_cancelled{false};
};

} // namespace calcium::packages
