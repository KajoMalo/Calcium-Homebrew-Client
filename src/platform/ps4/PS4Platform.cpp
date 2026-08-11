// PS4Platform.cpp — Orbis OS implementation of IPlatform.
//
// This file is compiled ONLY when CALCIUM_PLATFORM_PS4 is defined (PS4 SDK build).
// All SDK headers and libraries are provided by the OpenOrbis / PS4SDK toolchain.
//
// SDK references used:
//   libSceVideoOut     — display / video output
//   libSceUserService  — controller / user management
//   libSceAppInstUtil  — app installation queries
//   libSceSystemService — system-level service calls (launch, notification)
//   libSceNet / libSceHttp — networking
//   libSceKernel (POSIX compat layer) — filesystem

#ifdef CALCIUM_PLATFORM_PS4

#include "PS4Platform.hpp"
#include "../../filesystem/Filesystem.hpp"
#include "../../logging/Logger.hpp"

// ── PS4 SDK headers ──────────────────────────────────────────────────────────
// These are provided by the Orbis SDK or OpenOrbis equivalent.
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <orbis/AppInstUtil.h>
#include <orbis/Net.h>
#include <orbis/Http.h>
#include <orbis/Ssl.h>

// ─── PS4HttpClient forward declaration ───────────────────────────────────────
// Declared below; uses libSceHttp internally.
namespace calcium::networking {
    class PS4HttpClient;
}

namespace calcium::platform {

static constexpr std::string_view TAG = "PS4Platform";

// ─── PS4 HTTP client (libSceHttp) ─────────────────────────────────────────────

} // namespace calcium::platform

// Bring in the PS4 HTTP client implementation here so it is only compiled for PS4.
#include "PS4HttpClient.inl"

namespace calcium::platform {

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool PS4Platform::init() {
    if (m_initialised) return true;

    // Initialise networking stack.
    OrbisNetInitParam netParam{};
    netParam.memory   = malloc(1 * 1024 * 1024);
    netParam.size     = 1 * 1024 * 1024;
    netParam.flags    = 0;
    if (sceNetInit(&netParam) < 0) {
        logging::Logger::instance().error(TAG, "sceNetInit failed");
        return false;
    }

    // SSL for HTTPS.
    if (sceSslInit(1 * 1024 * 1024) < 0) {
        logging::Logger::instance().error(TAG, "sceSslInit failed");
        sceNetTerm();
        return false;
    }

    // HTTP library.
    if (sceHttpInit(1 * 1024 * 1024) < 0) {
        logging::Logger::instance().error(TAG, "sceHttpInit failed");
        sceSslTerm();
        sceNetTerm();
        return false;
    }

    // User service (needed for controller input).
    OrbisUserServiceInitializeParams userParam{};
    userParam.priority = SCE_KERNEL_PRIO_FIFO_DEFAULT;
    if (sceUserServiceInitialize(&userParam) < 0) {
        logging::Logger::instance().warning(TAG, "sceUserServiceInitialize failed (non-fatal)");
    }

    // Video output.
    if (sceVideoOutOpen(ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0, nullptr) < 0) {
        logging::Logger::instance().error(TAG, "sceVideoOutOpen failed");
        sceHttpTerm();
        sceSslTerm();
        sceNetTerm();
        return false;
    }

    logging::Logger::instance().info(TAG, "PS4 platform initialised.");
    m_initialised = true;
    return true;
}

void PS4Platform::shutdown() {
    if (!m_initialised) return;
    sceHttpTerm();
    sceSslTerm();
    sceNetTerm();
    sceUserServiceTerminate();
    logging::Logger::instance().info(TAG, "PS4 platform shut down.");
    m_initialised = false;
}

PS4Platform::~PS4Platform() {
    shutdown();
}

// ─── Services ─────────────────────────────────────────────────────────────────

std::shared_ptr<filesystem::IFilesystem> PS4Platform::create_filesystem() {
    // The PS4's POSIX compatibility layer exposes standard file I/O under /mnt.
    // std::filesystem works against these paths via the libc wrapper in the SDK.
    return std::make_shared<filesystem::Filesystem>();
}

std::shared_ptr<networking::IHttpClient> PS4Platform::create_http_client() {
    return std::make_shared<networking::PS4HttpClient>();
}

// ─── System info ──────────────────────────────────────────────────────────────

SystemInfo PS4Platform::system_info() const {
    SystemInfo info;
    info.is_ps4      = true;
    info.is_desktop  = false;
    info.platform_name = "PS4";

    // Firmware version via sceKernelGetSystemSwVersion.
    OrbisKernelSwVersion swver{};
    swver.size = sizeof(swver);
    if (sceKernelGetSystemSwVersion(&swver) == 0) {
        info.os_version = swver.version_string;
    }

    // CPU model (Jaguar APU).
    info.cpu_model = "AMD Jaguar x86-64 (8 cores)";

    // RAM: PS4 has 8 GB GDDR5, shared CPU/GPU.
    info.total_ram_bytes = 8ULL * 1024 * 1024 * 1024;
    // sceKernelGetProcessParam or vm_statistics would give available RAM;
    // for now report total as a conservative estimate.
    info.free_ram_bytes  = info.total_ram_bytes / 2;

    return info;
}

DisplayInfo PS4Platform::display_info() const {
    DisplayInfo di;
    // PS4 outputs at 1920×1080 at the system level.
    di.width  = 1920;
    di.height = 1080;
    di.dpi    = 96;
    return di;
}

// ─── Paths ────────────────────────────────────────────────────────────────────

std::string PS4Platform::app_data_path() const {
    // On PS4 application data lives under /data/ relative to the app sandbox root.
    return "/data/calcium-client";
}

std::string PS4Platform::config_file_path() const {
    return "/data/calcium-client/config.json";
}

// ─── App launching ────────────────────────────────────────────────────────────

bool PS4Platform::launch_app(const std::string& install_path,
                               const std::string& content_id) {
    (void)install_path;

    if (content_id.empty()) {
        logging::Logger::instance().error(TAG, "launch_app: content_id is required on PS4.");
        return false;
    }

    logging::Logger::instance().info(TAG, "Launching content: " + content_id);

    // sceSystemServiceLaunchApp launches a title by Content ID.
    OrbisSystemServiceLaunchAppParam param{};
    param.size = sizeof(param);

    int ret = sceSystemServiceLaunchApp(content_id.c_str(), nullptr, &param);
    if (ret < 0) {
        logging::Logger::instance().error(TAG,
            "sceSystemServiceLaunchApp failed: 0x" +
            [ret]{ char buf[16]; snprintf(buf, sizeof(buf), "%08X", (unsigned)ret); return std::string(buf); }());
        return false;
    }

    return true;
}

// ─── Event loop ───────────────────────────────────────────────────────────────

bool PS4Platform::poll_events() {
    // On PS4 controller input comes from libScePad.
    // The UIManager owns the pad polling loop; here we only check for a
    // system-level quit signal from sceSystemService.
    OrbisSystemServiceStatus status{};
    if (sceSystemServiceGetStatus(&status) == 0) {
        if (status.eventNum > 0) {
            OrbisSystemServiceEvent event{};
            while (sceSystemServiceReceiveEvent(&event) == 0) {
                if (event.eventType == ORBIS_SYSTEM_SERVICE_EVENT_EXIT_REQUEST) {
                    logging::Logger::instance().info(TAG, "System exit requested.");
                    return false;
                }
            }
        }
    }
    return true;
}

void PS4Platform::present_frame() {
    // Frame submission is handled by the GNM/GNMX command buffer in the
    // renderer. This hook exists for symmetry with the desktop platform;
    // the actual flip is triggered by sceVideoOutSubmitFlip in the renderer.
}

// ─── Factory ──────────────────────────────────────────────────────────────────

std::unique_ptr<IPlatform> create_ps4_platform() {
    return std::make_unique<PS4Platform>();
}

} // namespace calcium::platform

#endif // CALCIUM_PLATFORM_PS4
