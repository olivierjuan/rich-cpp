cmake_minimum_required(VERSION 3.20)
project(rich_smoke_test LANGUAGES CXX)

# Locate the installed rich package. CI sets CMAKE_PREFIX_PATH to the install dir.
find_package(rich CONFIG REQUIRED)

add_executable(smoke_test main.cpp)
target_link_libraries(smoke_test PRIVATE rich::rich_core)
target_compile_features(smoke_test PRIVATE cxx_std_20)
