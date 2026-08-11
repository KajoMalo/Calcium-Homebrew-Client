# ─── Collect sources by subsystem ────────────────────────────────────────────

set(LOGGING_SOURCES
    logging/Logger.cpp
)

set(FILESYSTEM_SOURCES
    filesystem/Filesystem.cpp
)

set(CONFIG_SOURCES
    config/Config.cpp
    config/InstalledDatabase.cpp
)

set(NETWORKING_SOURCES
    networking/HttpClient.cpp
    networking/DownloadManager.cpp
)

set(REPOSITORY_SOURCES
    repository/MetadataParser.cpp
    repository/Repository.cpp
    repository/RepositoryManager.cpp
)

set(PACKAGES_SOURCES
    packages/PackageVerifier.cpp
    packages/PackageInstaller.cpp
    packages/PackageManager.cpp
)

set(CORE_SOURCES
    core/AppManager.cpp
    core/Launcher.cpp
    core/Application.cpp
)

set(UI_SOURCES
    ui/Theme.cpp
    ui/UIManager.cpp
    ui/widgets/Button.cpp
    ui/widgets/Label.cpp
    ui/widgets/ProgressBar.cpp
    ui/widgets/ListView.cpp
    ui/widgets/AppCard.cpp
    ui/screens/HomeScreen.cpp
    ui/screens/CatalogScreen.cpp
    ui/screens/AppDetailScreen.cpp
    ui/screens/InstalledScreen.cpp
    ui/screens/DownloadsScreen.cpp
    ui/screens/SettingsScreen.cpp
    ui/screens/AboutScreen.cpp
)

# ─── Platform sources ─────────────────────────────────────────────────────────
if(CALCIUM_PLATFORM_PS4)
    set(PLATFORM_SOURCES
        platform/ps4/PS4Platform.cpp
    )
    set(PLATFORM_DEFINITIONS CALCIUM_PLATFORM_PS4)
else()
    set(PLATFORM_SOURCES
        platform/desktop/DesktopPlatform.cpp
    )
    set(PLATFORM_DEFINITIONS CALCIUM_PLATFORM_DESKTOP)
endif()

# ─── Core static library ─────────────────────────────────────────────────────
add_library(calcium_core STATIC
    ${LOGGING_SOURCES}
    ${FILESYSTEM_SOURCES}
    ${CONFIG_SOURCES}
    ${NETWORKING_SOURCES}
    ${REPOSITORY_SOURCES}
    ${PACKAGES_SOURCES}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${PLATFORM_SOURCES}
)

target_include_directories(calcium_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(calcium_core
    PUBLIC ${PLATFORM_DEFINITIONS}
)

target_link_libraries(calcium_core
    PUBLIC
        nlohmann_json::nlohmann_json
)

if(CALCIUM_ENABLE_CURL AND NOT CALCIUM_PLATFORM_PS4)
    target_link_libraries(calcium_core PUBLIC CURL::libcurl)
    target_compile_definitions(calcium_core PUBLIC CALCIUM_USE_CURL)
endif()

# zlib is required for ZIP deflate decompression in PackageInstaller.
find_package(ZLIB REQUIRED)
target_link_libraries(calcium_core PUBLIC ZLIB::ZLIB)

if(CALCIUM_BUILD_DESKTOP)
    if(TARGET SDL2::SDL2)
        target_link_libraries(calcium_core PUBLIC SDL2::SDL2)
    elseif(SDL2_FOUND)
        target_include_directories(calcium_core PUBLIC ${SDL2_INCLUDE_DIRS})
        target_link_libraries(calcium_core PUBLIC ${SDL2_LIBRARIES})
    endif()
    target_compile_definitions(calcium_core PUBLIC CALCIUM_DESKTOP_SDL)
endif()

# ─── Executable ──────────────────────────────────────────────────────────────
add_executable(calcium_client
    main.cpp
)

target_link_libraries(calcium_client PRIVATE calcium_core)

set_target_properties(calcium_client PROPERTIES
    OUTPUT_NAME "calcium-client"
)
