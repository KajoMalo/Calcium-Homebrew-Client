#pragma once

#include "IPackageManager.hpp"
#include "PackageVerifier.hpp"
#include "PackageInstaller.hpp"
#include "../networking/DownloadManager.hpp"
#include "../config/InstalledDatabase.hpp"
#include "../config/Config.hpp"
#include "../filesystem/IFilesystem.hpp"

#include <memory>
#include <atomic>
#include <mutex>

namespace calcium::packages {

/// Orchestrates the full install/uninstall pipeline:
///   Download → Verify → Install → Register in database
///
/// All operations are synchronous and intended to be called from a
/// background thread. The UI subscribes to ProgressCallback for updates.
class PackageManager final : public IPackageManager {
public:
    PackageManager(
        std::shared_ptr<networking::IHttpClient>  http_client,
        std::shared_ptr<filesystem::IFilesystem>  fs,
        std::shared_ptr<config::InstalledDatabase> db,
        const config::AppConfig&                  cfg
    );

    PackageResult install(const repository::AppMetadata& meta,
                          ProgressCallback on_progress = nullptr) override;

    PackageResult uninstall(const std::string& app_id,
                            ProgressCallback on_progress = nullptr) override;

    void cancel() override;
    bool is_busy() const override;

private:
    void emit(ProgressCallback& cb, const PackageProgress& pp);

    std::shared_ptr<networking::IHttpClient>   m_http;
    std::shared_ptr<filesystem::IFilesystem>   m_fs;
    std::shared_ptr<config::InstalledDatabase> m_db;
    config::AppConfig                          m_cfg;

    PackageInstaller   m_installer;
    std::atomic<bool>  m_busy{false};
    std::atomic<bool>  m_cancel_requested{false};
};

} // namespace calcium::packages
