add_library(panel INTERFACE)
target_include_directories(panel INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(panel INTERFACE rich_core)