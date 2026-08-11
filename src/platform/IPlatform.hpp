#pragma once

#include <string>
#include <memory>
#include <cstdint>

namespace calcium::filesystem { class IFilesystem; }
namespace calcium::networking  { class IHttpClient;  }

namespace calcium::platform {

/// System information reported by the platform layer.
struct SystemInfo {
    std::string platform_name;      ///< e.g. "Desktop (Linux)", "PS4"
    std::string os_version;         ///< e.g. "Ubuntu 22.04", "10.01"
    std::string cpu_model;
    uint64_t    total_ram_bytes = 0;
    uint64_t    free_ram_bytes  = 0;
    bool        is_ps4          = false;
    bool        is_desktop      = false;
};

/// Display / window size.
struct DisplayInfo {
    int width  = 1920;
    int height = 1080;
    int dpi    = 96;
};

/// Abstract factory and runtime services for a target platform.
///
/// The concrete implementation (DesktopPlatform or PS4Platform) is selected
/// at compile time via CALCIUM_PLATFORM_DESKTOP / CALCIUM_PLATFORM_PS4.
/// All platform-specific logic lives behind this interface; the rest of the
/// codebase never includes platform headers directly.
class IPlatform {
public:
    virtual ~IPlatform() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────
    /// One-time initialisation. Called before any other subsystem starts.
    virtual bool init() = 0;
    /// Tear down platform resources. Called after the main loop exits.
    virtual void shutdown() = 0;

    // ── Services ──────────────────────────────────────────────────────────
    /// Platform-native filesystem implementation.
    virtual std::shared_ptr<filesystem::IFilesystem> create_filesystem() = 0;
    /// Platform-native HTTP client implementation.
    virtual std::shared_ptr<networking::IHttpClient>  create_http_client() = 0;

    // ── System queries ────────────────────────────────────────────────────
    virtual SystemInfo  system_info()  const = 0;
    virtual DisplayInfo display_info() const = 0;

    /// Suggested path for the application's data directory.
    virtual std::string app_data_path() const = 0;
    /// Suggested path for the application configuration file.
    virtual std::string config_file_path() const = 0;

    // ── Process management ────────────────────────────────────────────────
    /// Launch an installed application by its install path.
    /// On PS4 this uses the sceAppInstUtil / sceSystemService launch APIs.
    /// On desktop this spawns the process via the OS.
    /// Returns true if the launch request was accepted.
    virtual bool launch_app(const std::string& install_path,
                            const std::string& content_id) = 0;

    // ── UI / window ───────────────────────────────────────────────────────
    /// Poll and dispatch input events. Returns false when the app should quit.
    virtual bool poll_events() = 0;
    /// Present the rendered frame to the display.
    virtual void present_frame() = 0;

    // ── Factory ───────────────────────────────────────────────────────────
    /// Create the platform instance appropriate for the current build target.
    static std::unique_ptr<IPlatform> create();
};

} // namespace calcium::platform
