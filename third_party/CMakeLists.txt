# ─── nlohmann/json ────────────────────────────────────────────────────────────
# Header-only JSON library. Fetched via FetchContent so no submodule required.
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

# ─── Catch2 (testing) ─────────────────────────────────────────────────────────
FetchContent_Declare(
    Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
    URL_HASH SHA256=3b9b438e6ac1c8e5dfcf3d7e5e38f44a6de5cff97b71aa916b5b0dc52f8dda2a
)
FetchContent_MakeAvailable(Catch2)

# ─── SDL2 (desktop simulation display) ────────────────────────────────────────
if(CALCIUM_BUILD_DESKTOP)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        message(STATUS "SDL2 not found via find_package; attempting FetchContent...")
        FetchContent_Declare(
            SDL2
            URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.2/SDL2-2.30.2.tar.gz
            URL_HASH SHA256=891d7d75b9975736916b477ca1a47fe706de0f9e57cec58f69d9b23b527b1e30
        )
        FetchContent_MakeAvailable(SDL2)
    endif()
endif()

# ─── libcurl (HTTP networking) ────────────────────────────────────────────────
if(CALCIUM_ENABLE_CURL AND NOT CALCIUM_PLATFORM_PS4)
    find_package(CURL REQUIRED)
endif()
