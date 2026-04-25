cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(back-tester VERSION 0.0.1 DESCRIPTION "CMF back-tester" LANGUAGES CXX C)

# Optional Apache Arrow C++ for direct .feather ingestion. When OFF the
# project builds without any Arrow dependency; the Python script remains
# the primary Feather producer either way.
option(ENABLE_ARROW "Build the C++ Arrow Feather producer (heavy, ~5 min FetchContent)" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

include(cmake/GlobalSettings.cmake)
include(cmake/ThirdPartyLibs.cmake)
include(cmake/GenerateVersionFile.cmake)

add_subdirectory(src)
add_subdirectory(config)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
