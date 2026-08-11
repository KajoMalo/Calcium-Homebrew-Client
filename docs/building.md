# Building Calcium Client

## Prerequisites

### All platforms
- CMake 3.20 or later
- A C++17-capable compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Git (for FetchContent dependency download)
- Internet access on first build (nlohmann/json, Catch2, optionally SDL2 are fetched automatically)

### Desktop build (Linux)
```bash
# Ubuntu / Debian
sudo apt install cmake build-essential libcurl4-openssl-dev zlib1g-dev \
                 libsdl2-dev libsdl2-ttf-dev

# Fedora / RHEL
sudo dnf install cmake gcc-c++ libcurl-devel zlib-devel SDL2-devel SDL2_ttf-devel

# Arch Linux
sudo pacman -S cmake curl zlib sdl2 sdl2_ttf
```

### Desktop build (macOS)
```bash
brew install cmake curl zlib sdl2 sdl2_ttf
```

### Desktop build (Windows)
Install via [vcpkg](https://vcpkg.io):
```powershell
vcpkg install curl zlib sdl2 sdl2-ttf
```
Then pass `-DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake` to CMake.

### PS4 build
- OpenOrbis PS4 toolchain or equivalent homebrew SDK installed at `$OO_PS4_TOOLCHAIN`
- Set `-DCALCIUM_PLATFORM_PS4=ON -DCALCIUM_BUILD_DESKTOP=OFF -DCALCIUM_ENABLE_CURL=OFF`

---

## CMake options

| Option | Default | Description |
|---|---|---|
| `CALCIUM_BUILD_TESTS` | `ON` | Build the Catch2 unit test suite |
| `CALCIUM_BUILD_DESKTOP` | `ON` | Build the SDL2 desktop simulation mode |
| `CALCIUM_PLATFORM_PS4` | `OFF` | Build for PS4 (Orbis SDK target) |
| `CALCIUM_ENABLE_CURL` | `ON` | Use libcurl for HTTP (desktop only) |

---

## Desktop build (recommended for development)

```bash
cd CalciumClient

# Configure
cmake -B build \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCALCIUM_BUILD_DESKTOP=ON \
      -DCALCIUM_BUILD_TESTS=ON

# Build
cmake --build build --parallel

# Run with the bundled mock repository
./build/bin/calcium-client --config resources/config.default.json
```

The application opens a 1280×720 SDL2 window with the full UI. The mock repository at `resources/mock_repo/index.json` is loaded automatically via the `file://` scheme — no network connection is needed.

---

## Release build

```bash
cmake -B build-release \
      -DCMAKE_BUILD_TYPE=Release \
      -DCALCIUM_BUILD_TESTS=OFF

cmake --build build-release --parallel

# Install to a staging directory
cmake --install build-release --prefix /opt/calcium-client
```

---

## PS4 build

```bash
cmake -B build-ps4 \
      -DCMAKE_BUILD_TYPE=Release \
      -DCALCIUM_PLATFORM_PS4=ON \
      -DCALCIUM_BUILD_DESKTOP=OFF \
      -DCALCIUM_ENABLE_CURL=OFF \
      -DCMAKE_TOOLCHAIN_FILE=$OO_PS4_TOOLCHAIN/cmake/toolchain.cmake

cmake --build build-ps4 --parallel
```

This produces a PS4 homebrew ELF. Package it using the tools provided by your homebrew SDK.

---

## Running the desktop simulation

```bash
# With the default mock repository (offline, no network needed):
./build/bin/calcium-client --config resources/config.default.json

# With a custom config file:
./build/bin/calcium-client --config /path/to/my-config.json

# Print help:
./build/bin/calcium-client --help
```

### Keyboard shortcuts (desktop)
| Key | Action |
|---|---|
| `Escape` | Go back / close overlay |
| `Backspace` | Go back (on detail screen) |
| `↑` / `↓` | Navigate list items |
| `Enter` | Confirm / select |
| Mouse wheel | Scroll lists and catalog |

---

## Output directories

After a successful build:

```
build/
  bin/
    calcium-client       # Main executable
  lib/
    libcalcium_core.a    # Static core library
```

---

## Troubleshooting

**SDL2 not found**
Pass `-DSDL2_DIR=/path/to/sdl2/cmake` or install SDL2 via your package manager.

**libcurl not found**
Pass `-DCURL_DIR=/path/to/curl` or set `CURL_ROOT`. On Windows use vcpkg.

**FetchContent fails to download**
You may be behind a proxy. Set `HTTPS_PROXY` in your environment, or manually place the archives in `build/_deps/` before configuring.

**Font not found at runtime**
Calcium searches for a system font automatically. Place any `.ttf` file at `resources/themes/font.ttf` to use it as the preferred font.
