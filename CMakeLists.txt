cmake_minimum_required(VERSION 3.20)
project(rich LANGUAGES CXX VERSION 0.0.1)

set(RICH_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/rich-cpp)
include_directories(${RICH_SRC})

# Core library
add_library(rich_core SHARED src/rich-cpp/core/rich_core.cpp)
target_include_directories(rich_core PUBLIC
    $<BUILD_INTERFACE:${RICH_SRC}/core>
    $<INSTALL_INTERFACE:include/rich>)
target_compile_features(rich_core PUBLIC cxx_std_20)

# Panel mini demo
add_subdirectory(src/rich-cpp/panel)

# Demo binaries
add_executable(run_quick src/rich-cpp/core/quick.cpp)
target_link_libraries(run_quick PRIVATE rich_core panel)

add_executable(run_columns src/rich-cpp/examples/columns_demo.cpp)
target_link_libraries(run_columns PRIVATE rich_core panel)

# Oracle validation test runner
add_executable(test_runner src/rich-cpp/tests/test_runner.cpp)
target_link_libraries(test_runner PRIVATE rich_core panel)

# Throwaway verification for the track range adapter (not part of the oracle
# suite; delete once the adapter is exercised by a demo or example).
add_executable(verify_track src/rich-cpp/scripts/verify_track.cpp)
target_link_libraries(verify_track PRIVATE rich_core)

# ── Install & export ──────────────────────────────────────────────────────────
include(GNUInstallDirs)

install(TARGETS rich_core panel
        EXPORT rich-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY src/rich-cpp/core/ src/rich-cpp/panel/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rich
        FILES_MATCHING PATTERN "*.h")

install(EXPORT rich-targets
        FILE rich-targets.cmake
        NAMESPACE rich::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rich)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/rich-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/rich-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/rich-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rich)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/rich-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/rich-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rich)

# Downstream smoke test lives in examples/find_package_smoke/ and is verified
# by CI after `cmake --install` (see .github/workflows/ci.yml).




