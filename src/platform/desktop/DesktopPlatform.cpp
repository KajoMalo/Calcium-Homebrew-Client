#include "DesktopPlatform.hpp"
#include "../../filesystem/Filesystem.hpp"
#include "../../networking/HttpClient.hpp"
#include "../../logging/Logger.hpp"

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__APPLE__)
#  include <sys/sysctl.h>
#  include <mach/mach.h>
#else
#  include <sys/sysinfo.h>
#  include <fstream>
#endif

namespace calcium::platform {

static constexpr std::string_view TAG = "DesktopPlatform";

// ─── Lifecycle ────────────────────────────────────────────────────────────────

bool DesktopPlatform::init() {
    if (m_initialised) return true;

#ifdef CALCIUM_DESKTOP_SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        logging::Logger::instance().error(TAG,
            std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }

    // Query the primary display resolution before creating the window.
    SDL_DisplayMode dm{};
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        // Use 80% of the display resolution for the window.
        m_display_width  = dm.w * 4 / 5;
        m_display_height = dm.h * 4 / 5;
    }

    m_window = SDL_CreateWindow(
        "Calcium Client",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_display_width, m_display_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!m_window) {
        logging::Logger::instance().error(TAG,
            std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        SDL_Quit();
        return false;
    }

    m_renderer = SDL_CreateRenderer(
        m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!m_renderer) {
        // Fallback to software renderer.
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!m_renderer) {
        logging::Logger::instance().error(TAG,
            std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        SDL_DestroyWindow(m_window);
        SDL_Quit();
        return false;
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    logging::Logger::instance().info(TAG,
        "SDL2 window created (" + std::to_string(m_display_width) +
        "x" + std::to_string(m_display_height) + ")");
#else
    logging::Logger::instance().info(TAG, "Headless desktop mode (no SDL2).");
#endif

    m_initialised = true;
    return true;
}

void DesktopPlatform::shutdown() {
    if (!m_initialised) return;
#ifdef CALCIUM_DESKTOP_SDL
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)   { SDL_DestroyWindow(m_window);     m_window   = nullptr; }
    SDL_Quit();
    logging::Logger::instance().info(TAG, "SDL2 shut down.");
#endif
    m_initialised = false;
}

DesktopPlatform::~DesktopPlatform() {
    shutdown();
}

// ─── Services ─────────────────────────────────────────────────────────────────

std::shared_ptr<filesystem::IFilesystem> DesktopPlatform::create_filesystem() {
    return std::make_shared<filesystem::Filesystem>();
}

std::shared_ptr<networking::IHttpClient> DesktopPlatform::create_http_client() {
    return std::make_shared<networking::HttpClient>();
}

// ─── System info ──────────────────────────────────────────────────────────────

SystemInfo DesktopPlatform::system_info() const {
    SystemInfo info;
    info.is_desktop = true;
    info.is_ps4     = false;

#if defined(_WIN32)
    info.platform_name = "Desktop (Windows)";
    OSVERSIONINFOEXW osvi{};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    // RtlGetVersion is safe and doesn't lie about Win10+ like GetVersionEx.
    using RtlGetVersionFn = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
    auto* fn = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    if (fn) fn(reinterpret_cast<PRTL_OSVERSIONINFOW>(&osvi));
    info.os_version = "Windows " +
        std::to_string(osvi.dwMajorVersion) + "." +
        std::to_string(osvi.dwMinorVersion) + "." +
        std::to_string(osvi.dwBuildNumber);

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    info.total_ram_bytes = ms.ullTotalPhys;
    info.free_ram_bytes  = ms.ullAvailPhys;

    // CPU name from registry.
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        R"(HARDWARE\DESCRIPTION\System\CentralProcessor\0)",
        0, KEY_READ, &key) == ERROR_SUCCESS) {
        char buf[256]{};
        DWORD sz = sizeof(buf);
        RegQueryValueExA(key, "ProcessorNameString", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(buf), &sz);
        RegCloseKey(key);
        info.cpu_model = buf;
    }

#elif defined(__APPLE__)
    info.platform_name = "Desktop (macOS)";
    // os version from sysctl
    char osrel[64]{};
    std::size_t sz = sizeof(osrel);
    sysctlbyname("kern.osrelease", osrel, &sz, nullptr, 0);
    info.os_version = std::string("macOS kernel ") + osrel;
    // RAM
    uint64_t ram = 0;
    sz = sizeof(ram);
    sysctlbyname("hw.memsize", &ram, &sz, nullptr, 0);
    info.total_ram_bytes = ram;
    // Available RAM via mach
    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vms{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    host_statistics64(host, HOST_VM_INFO64,
                      reinterpret_cast<host_info64_t>(&vms), &count);
    info.free_ram_bytes = static_cast<uint64_t>(vms.free_count) * 4096ULL;
    // CPU
    char cpu[128]{};
    sz = sizeof(cpu);
    sysctlbyname("machdep.cpu.brand_string", cpu, &sz, nullptr, 0);
    info.cpu_model = cpu;

#else
    info.platform_name = "Desktop (Linux)";
    // /etc/os-release for OS version
    {
        std::ifstream f("/etc/os-release");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("PRETTY_NAME=", 0) == 0) {
                info.os_version = line.substr(12);
                // Strip surrounding quotes if present.
                if (!info.os_version.empty() && info.os_version.front() == '"') {
                    info.os_version = info.os_version.substr(1, info.os_version.size() - 2);
                }
                break;
            }
        }
    }
    // RAM from sysinfo
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        info.total_ram_bytes = static_cast<uint64_t>(si.totalram) * si.mem_unit;
        info.free_ram_bytes  = static_cast<uint64_t>(si.freeram)  * si.mem_unit;
    }
    // CPU from /proc/cpuinfo
    {
        std::ifstream f("/proc/cpuinfo");
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("model name", 0) == 0) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    info.cpu_model = line.substr(colon + 2);
                }
                break;
            }
        }
    }
#endif

    return info;
}

DisplayInfo DesktopPlatform::display_info() const {
    DisplayInfo di;
    di.width  = m_display_width;
    di.height = m_display_height;
#ifdef CALCIUM_DESKTOP_SDL
    if (m_window) {
        SDL_GetWindowSize(m_window, &di.width, &di.height);
        float ddpi = 96.0f;
        SDL_GetDisplayDPI(0, nullptr, &ddpi, nullptr);
        di.dpi = static_cast<int>(ddpi);
    }
#endif
    return di;
}

// ─── Paths ────────────────────────────────────────────────────────────────────

std::string DesktopPlatform::app_data_path() const {
    filesystem::Filesystem fs;
    return fs.app_data_directory().string();
}

std::string DesktopPlatform::config_file_path() const {
    return (std::filesystem::path(app_data_path()) / "config.json").string();
}

// ─── App launching ────────────────────────────────────────────────────────────

bool DesktopPlatform::launch_app(const std::string& install_path,
                                  const std::string& content_id) {
    (void)content_id; // not used on desktop

    logging::Logger::instance().info(TAG, "Launching app from: " + install_path);

    // Look for an executable inside the install directory.
    // On Linux/macOS: the file named "eboot.bin" or an executable with the
    // same name as the directory. On Windows: any .exe in the directory.
    std::filesystem::path dir{install_path};
    if (!std::filesystem::exists(dir)) {
        logging::Logger::instance().error(TAG, "Install path does not exist: " + install_path);
        return false;
    }

    std::filesystem::path exe;
#if defined(_WIN32)
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.path().extension() == ".exe") {
            exe = entry.path();
            break;
        }
    }
#else
    // Try common names.
    for (const auto& name : {"eboot.bin", "eboot", "run.sh", "start.sh"}) {
        auto candidate = dir / name;
        if (std::filesystem::exists(candidate)) {
            exe = candidate;
            break;
        }
    }
    // Fall back to any executable file.
    if (exe.empty()) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                auto perms = entry.status().permissions();
                using p = std::filesystem::perms;
                if ((perms & p::owner_exec) != p::none) {
                    exe = entry.path();
                    break;
                }
            }
        }
    }
#endif

    if (exe.empty()) {
        logging::Logger::instance().error(TAG, "No launchable executable found in: " + install_path);
        return false;
    }

    logging::Logger::instance().info(TAG, "Executing: " + exe.string());

#if defined(_WIN32)
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::string cmd = "\"" + exe.string() + "\"";
    bool ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                              FALSE, 0, nullptr,
                              dir.string().c_str(), &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok;
#else
    // Use fork+execv on POSIX.
    pid_t pid = fork();
    if (pid == 0) {
        // Child: change directory, then execute.
        chdir(dir.string().c_str());
        char* argv[] = { const_cast<char*>(exe.string().c_str()), nullptr };
        execv(exe.string().c_str(), argv);
        std::exit(1); // execv only returns on failure
    }
    return pid > 0;
#endif
}

// ─── Event loop helpers ───────────────────────────────────────────────────────

bool DesktopPlatform::poll_events() {
#ifdef CALCIUM_DESKTOP_SDL
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return false;
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE) return false;
    }
    return true;
#else
    return true; // headless — never quits unless shutdown() is called
#endif
}

void DesktopPlatform::present_frame() {
#ifdef CALCIUM_DESKTOP_SDL
    if (m_renderer) SDL_RenderPresent(m_renderer);
#endif
}

// ─── Factory ──────────────────────────────────────────────────────────────────

std::unique_ptr<IPlatform> IPlatform::create() {
#if defined(CALCIUM_PLATFORM_PS4)
    // PS4Platform is declared in platform/ps4/PS4Platform.hpp
    extern std::unique_ptr<IPlatform> create_ps4_platform();
    return create_ps4_platform();
#else
    return std::make_unique<DesktopPlatform>();
#endif
}

} // namespace calcium::platform
