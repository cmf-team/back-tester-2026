cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(back-tester VERSION 0.0.1 DESCRIPTION "CMF back-tester" LANGUAGES CXX C)

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

option(BUILD_BENCHMARKS "Build Google Benchmark targets" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
