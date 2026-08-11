cmake_minimum_required(VERSION 3.20)

# All test targets link against calcium_core and Catch2.
# Each .cpp file is its own test executable so failures are isolated.

set(TEST_SOURCES
    test_metadata_parser.cpp
    test_config.cpp
    test_installed_database.cpp
    test_package_verifier.cpp
    test_download_state.cpp
    test_repository.cpp
    test_error_handling.cpp
)

foreach(src ${TEST_SOURCES})
    get_filename_component(target_name ${src} NAME_WE)
    add_executable(${target_name} ${src})
    target_link_libraries(${target_name}
        PRIVATE
            calcium_core
            Catch2::Catch2WithMain
    )
    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME ${target_name} COMMAND ${target_name})
endforeach()

# Copy mock repo fixtures to the test binary directory so file:// paths resolve.
add_custom_target(copy_test_fixtures ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/mock_repo
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/mock_repo
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/resources/config.default.json
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.default.json
)
