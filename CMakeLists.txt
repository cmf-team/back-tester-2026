cmake_minimum_required(VERSION 3.18)
project(backtester-2026 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ─────────────────────────────────────────────────────────
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Release: maximum speed
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native)
endif()

# ── Include dirs ───────────────────────────────────────────────────────────
include_directories(include)

# ── Standard task ──────────────────────────────────────────────────────────
add_executable(standard_task src/standard_task.cpp)
target_include_directories(standard_task PRIVATE include)

# ── Hard task ──────────────────────────────────────────────────────────────
add_executable(hard_task src/hard_task.cpp)
target_include_directories(hard_task PRIVATE include)
find_package(Threads REQUIRED)
target_link_libraries(hard_task PRIVATE Threads::Threads)

# ── Install ────────────────────────────────────────────────────────────────
install(TARGETS standard_task hard_task RUNTIME DESTINATION bin)
