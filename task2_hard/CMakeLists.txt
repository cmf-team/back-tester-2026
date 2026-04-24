cmake_minimum_required(VERSION 3.22)
project(backtester VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -fsanitize=address,undefined")

# ── Third-party: simdjson ──────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG        v3.9.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(simdjson)

# ── Third-party: Catch2 v2 ────────────────────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v2.13.10
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── Threads ───────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── Common library (shared between app and tests) ─────────────────────────
add_library(common STATIC
    src/common/EventParser.cpp
    src/common/LimitOrderBook.cpp
)
target_include_directories(common PUBLIC src/common)
target_link_libraries(common PUBLIC simdjson Threads::Threads)

if(MSVC)
    target_compile_options(common PRIVATE /W4)
else()
    target_compile_options(common PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ── Main executable ───────────────────────────────────────────────────────
add_executable(backtester src/main/main.cpp)
target_link_libraries(backtester PRIVATE common)

# ── Tests ─────────────────────────────────────────────────────────────────
enable_testing()

add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE common Catch2::Catch2)
target_include_directories(tests PRIVATE src/common)

include(CTest)
add_test(NAME unit_tests COMMAND tests)

# ── Optional: Apache Arrow C++ for Feather reading ────────────────────────
# Uncomment if Arrow is installed (brew install apache-arrow / apt install libarrow-dev)
# find_package(Arrow QUIET)
# if(Arrow_FOUND)
#     message(STATUS "Arrow found — Feather C++ reader enabled")
#     add_executable(feather_reader src/feather/feather_main.cpp)
#     target_link_libraries(feather_reader PRIVATE common Arrow::arrow_shared)
# endif()
