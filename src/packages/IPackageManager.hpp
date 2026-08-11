#pragma once

#include "../repository/AppMetadata.hpp"

#include <string>
#include <functional>
#include <cstdint>

namespace calcium::packages {

/// Granular status of a package operation.
enum class PackageOpStatus {
    Idle,
    Downloading,
    Verifying,
    Installing,
    Completed,
    Failed,
    Cancelled,
};

inline constexpr std::string_view to_string(PackageOpStatus s) {
    switch (s) {
        case PackageOpStatus::Idle:        return "Idle";
        case PackageOpStatus::Downloading: return "Downloading";
        case PackageOpStatus::Verifying:   return "Verifying";
        case PackageOpStatus::Installing:  return "Installing";
        case PackageOpStatus::Completed:   return "Completed";
        case PackageOpStatus::Failed:      return "Failed";
        case PackageOpStatus::Cancelled:   return "Cancelled";
    }
    return "Unknown";
}

/// Progress report emitted during install/uninstall operations.
struct PackageProgress {
    std::string    app_id;
    PackageOpStatus status          = PackageOpStatus::Idle;
    float          progress         = 0.0f;  ///< 0.0–1.0
    uint64_t       bytes_received   = 0;
    uint64_t       bytes_total      = 0;
    std::string    status_message;           ///< Human-readable current step.
    std::string    error_message;            ///< Non-empty on failure.
};

/// Result of a package operation.
struct PackageResult {
    bool        success = false;
    std::string error;
    std::string app_id;

    static PackageResult ok(std::string id)          { return {true,  {},            std::move(id)}; }
    static PackageResult fail(std::string id,
                              std::string err)        { return {false, std::move(err), std::move(id)}; }
};

using ProgressCallback = std::function<void(const PackageProgress&)>;

/// Abstract package manager interface.
class IPackageManager {
public:
    virtual ~IPackageManager() = default;

    /// Download, verify, and install a package. Blocking.
    /// Progress is reported via on_progress throughout.
    virtual PackageResult install(const repository::AppMetadata& meta,
                                  ProgressCallback on_progress = nullptr) = 0;

    /// Remove an installed application.
    virtual PackageResult uninstall(const std::string& app_id,
                                    ProgressCallback on_progress = nullptr) = 0;

    /// Cancel the current install/uninstall operation if one is running.
    virtual void cancel() = 0;

    /// Returns true if an operation is currently in progress.
    virtual bool is_busy() const = 0;
};

} // namespace calcium::packages
