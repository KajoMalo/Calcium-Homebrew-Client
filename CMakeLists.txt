cmake_minimum_required(VERSION 3.20)

project(CalciumClient
    VERSION 1.0.0
    DESCRIPTION "Calcium Client - PS4 Homebrew Software Distribution Client"
    LANGUAGES CXX
)

# ─── C++ standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Build options ────────────────────────────────────────────────────────────
option(CALCIUM_BUILD_TESTS   "Build unit tests"               ON)
option(CALCIUM_BUILD_DESKTOP "Build desktop simulation mode"  ON)
option(CALCIUM_PLATFORM_PS4  "Build for PS4 platform target"  OFF)
option(CALCIUM_ENABLE_CURL   "Use libcurl for HTTP client"    ON)

# ─── Output directories ───────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ─── Compiler warnings ───────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ─── Third-party dependencies ─────────────────────────────────────────────────
add_subdirectory(third_party)

# ─── Core library (platform-independent) ─────────────────────────────────────
add_subdirectory(src)

# ─── Tests ───────────────────────────────────────────────────────────────────
if(CALCIUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ─── Install rules ───────────────────────────────────────────────────────────
install(TARGETS calcium_client
    RUNTIME DESTINATION bin
)
install(DIRECTORY resources/ DESTINATION share/calcium)
