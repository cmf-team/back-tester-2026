cmake_minimum_required(VERSION 3.15)
project(back_tester)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================
# MAIN PROGRAM
# =========================
add_executable(back-tester
    src/main/main.cpp
    src/common/JsonLineParser.cpp
    src/common/Summary.cpp
)

target_include_directories(back-tester PRIVATE
    src/common
)

# =========================
# TEST: parser
# =========================
add_executable(test_parser
    test/test_parser.cpp
    src/common/JsonLineParser.cpp
)

target_include_directories(test_parser PRIVATE
    src/common
)

# =========================
# TEST: summary
# =========================
add_executable(test_summary
    test/test_summary.cpp
    src/common/Summary.cpp
)

target_include_directories(test_summary PRIVATE
    src/common
)