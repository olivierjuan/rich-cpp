cmake_minimum_required(VERSION 3.20)
project(rich LANGUAGES CXX VERSION 0.0.1)
set(RICH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(RICH_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Core library
add_library(rich_core SHARED core/rich_core.cpp)
target_include_directories(rich_core PUBLIC ${RICH_ROOT}/core)
target_compile_features(rich_core PUBLIC cxx_std_20)

# Panel mini demo
add_subdirectory(panel)

# Demo binaries
add_executable(run_quick core/quick.cpp)
target_link_libraries(run_quick PRIVATE rich_core panel)

add_executable(run_columns examples/columns_demo.cpp)
target_link_libraries(run_columns PRIVATE rich_core panel)

# Oracle validation test runner
add_executable(test_runner tests/test_runner.cpp)
target_link_libraries(test_runner PRIVATE rich_core panel)
