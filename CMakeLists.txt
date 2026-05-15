cmake_minimum_required(VERSION 3.16)

project(market_data_ingestion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)

set(INGEST_SOURCES
    src/app/ArgsParser.cpp
    src/domain/MarketDataEvent.cpp
    src/io/MmapFile.cpp
    src/parsing/JsonParser.cpp
    src/processing/LoggingMarketDataEventProcessor.cpp
    src/runners/FlatMergeRunner.cpp
    src/runners/HardRunnerSupport.cpp
    src/runners/HierarchicalMergeRunner.cpp
    src/runners/InputFileDiscovery.cpp
    src/runners/ResultPrinter.cpp
    src/runners/StandardRunner.cpp
    third_party/simdjson/simdjson.cpp
)

add_library(ingest_core STATIC ${INGEST_SOURCES})
target_include_directories(ingest_core PUBLIC src)
target_include_directories(ingest_core PRIVATE third_party/simdjson)
target_link_libraries(ingest_core PUBLIC Threads::Threads)

add_executable(ingest src/main.cpp)
target_link_libraries(ingest PRIVATE ingest_core)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ingest_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(ingest PRIVATE -Wall -Wextra -Wpedantic)
    set_source_files_properties(third_party/simdjson/simdjson.cpp PROPERTIES
        COMPILE_FLAGS "-Wno-error -w")

    if (ENABLE_COVERAGE)
        target_compile_options(ingest_core PUBLIC -O0 -g --coverage)
        target_compile_options(ingest PRIVATE -O0 -g --coverage)
        target_link_options(ingest_core PUBLIC --coverage)
        target_link_options(ingest PRIVATE --coverage)
    endif()
endif()

enable_testing()
add_executable(ingest_tests
    tests/test_main.cpp
    tests/app/ArgsParserTest.cpp
    tests/concurrency/NonBlockingQueueTest.cpp
    tests/domain/MarketDataEventTest.cpp
    tests/parsing/JsonParserTest.cpp
    tests/runners/FlatMergeRunnerTest.cpp
    tests/runners/HierarchicalMergeRunnerTest.cpp
    tests/runners/StandardRunnerTest.cpp
)
target_link_libraries(ingest_tests PRIVATE ingest_core)
target_include_directories(ingest_tests PRIVATE tests)
target_compile_definitions(ingest_tests PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data")
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ingest_tests PRIVATE -Wall -Wextra -Wpedantic)
    if (ENABLE_COVERAGE)
        target_compile_options(ingest_tests PRIVATE -O0 -g --coverage)
        target_link_options(ingest_tests PRIVATE --coverage)
    endif()
endif()
add_test(NAME ingest_tests COMMAND ingest_tests)

add_test(
    NAME ingest_smoke_standard
    COMMAND ingest --mode standard --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/single_valid.ndjson --print-events 0
)
set_tests_properties(ingest_smoke_standard PROPERTIES
    PASS_REGULAR_EXPRESSION "Summary[\r\n]+total_messages_processed=2[\r\n]+first_timestamp=[0-9]+[\r\n]+last_timestamp=[0-9]+"
)

add_test(
    NAME ingest_smoke_flat
    COMMAND ingest --mode flat --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/multi --print-events 0
)
set_tests_properties(ingest_smoke_flat PROPERTIES
    PASS_REGULAR_EXPRESSION "chronological_violations=0"
)

add_test(
    NAME ingest_smoke_hierarchy
    COMMAND ingest --mode hierarchy --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data/multi --print-events 0
)
set_tests_properties(ingest_smoke_hierarchy PROPERTIES
    PASS_REGULAR_EXPRESSION "chronological_violations=0"
)

add_test(NAME ingest_smoke_help COMMAND ingest --help)
set_tests_properties(ingest_smoke_help PROPERTIES PASS_REGULAR_EXPRESSION "Usage:")
