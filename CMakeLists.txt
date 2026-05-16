cmake_minimum_required(VERSION 3.16)

project(market_data_ingestion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

option(ENABLE_COVERAGE "Build with gcov coverage instrumentation" OFF)
option(ENABLE_ARROW "Build optional Arrow/Feather input support" OFF)

set(INGEST_SOURCES
    src/app/ArgsParser.cpp
    src/backtest/BacktestMetrics.cpp
    src/backtest/BacktestReport.cpp
    src/backtest/ExecutionSimulator.cpp
    src/backtest/MarketView.cpp
    src/backtest/OrderManager.cpp
    src/backtest/Portfolio.cpp
    src/book/BookManager.cpp
    src/book/LimitOrderBook.cpp
    src/domain/MarketDataEvent.cpp
    src/io/MmapFile.cpp
    src/parsing/JsonParser.cpp
    src/processing/AsyncSnapshotWriter.cpp
    src/processing/BacktestMarketDataEventProcessor.cpp
    src/processing/LoggingMarketDataEventProcessor.cpp
    src/processing/LobMarketDataEventProcessor.cpp
    src/processing/ShardedLobMarketDataEventProcessor.cpp
    src/runners/BenchmarkRunner.cpp
    src/runners/FlatMergeRunner.cpp
    src/runners/HardRunnerSupport.cpp
    src/runners/HierarchicalMergeRunner.cpp
    src/runners/InputFileDiscovery.cpp
    src/runners/ResultPrinter.cpp
    src/runners/StandardRunner.cpp
    src/strategies/AvellanedaStoikovStrategy.cpp
    src/strategies/FixedQuoteStrategy.cpp
    src/strategies/MicropriceAvellanedaStoikovStrategy.cpp
    third_party/simdjson/simdjson.cpp
)

set(INGEST_TEST_SOURCES
    tests/test_main.cpp
    tests/app/ArgsParserTest.cpp
    tests/backtest/BacktestReportTest.cpp
    tests/backtest/BacktestTypesTest.cpp
    tests/backtest/ExecutionSimulatorTest.cpp
    tests/backtest/MarketViewTest.cpp
    tests/backtest/OrderManagerTest.cpp
    tests/backtest/PortfolioTest.cpp
    tests/book/BookManagerTest.cpp
    tests/book/LimitOrderBookTest.cpp
    tests/concurrency/NonBlockingQueueTest.cpp
    tests/domain/MarketDataEventTest.cpp
    tests/parsing/JsonParserTest.cpp
    tests/processing/AsyncSnapshotWriterTest.cpp
    tests/processing/BacktestMarketDataEventProcessorTest.cpp
    tests/processing/LobMarketDataEventProcessorTest.cpp
    tests/runners/FlatMergeRunnerTest.cpp
    tests/runners/HardLobIntegrationTest.cpp
    tests/runners/HierarchicalMergeRunnerTest.cpp
    tests/runners/ResultPrinterTest.cpp
    tests/runners/StandardRunnerLobIntegrationTest.cpp
    tests/runners/StandardRunnerTest.cpp
    tests/strategies/AvellanedaStoikovStrategyTest.cpp
    tests/strategies/FixedQuoteStrategyTest.cpp
    tests/strategies/MicropriceAvellanedaStoikovStrategyTest.cpp
    tests/strategies/StrategyTest.cpp
)

if (ENABLE_ARROW)
    find_package(Arrow CONFIG QUIET)
    if (Arrow_FOUND)
        message(STATUS "Arrow C++ found via CMake package")
        set(ARROW_TARGET Arrow::arrow_shared)
    else()
        find_package(Python3 COMPONENTS Interpreter REQUIRED)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} -c "import pathlib, pyarrow; print(pathlib.Path(pyarrow.__file__).parent)"
            OUTPUT_VARIABLE PYARROW_ROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE PYARROW_RESULT
        )
        if (NOT PYARROW_RESULT EQUAL 0)
            message(FATAL_ERROR "ENABLE_ARROW=ON requires Arrow C++ or a pyarrow wheel with bundled headers/libs")
        endif()

        set(PYARROW_INCLUDE_DIR "${PYARROW_ROOT}/include")
        file(GLOB PYARROW_ARROW_LIBS "${PYARROW_ROOT}/libarrow.so*")
        list(SORT PYARROW_ARROW_LIBS)
        if (NOT PYARROW_ARROW_LIBS)
            message(FATAL_ERROR "Could not find libarrow.so under pyarrow at ${PYARROW_ROOT}")
        endif()
        list(GET PYARROW_ARROW_LIBS 0 PYARROW_ARROW_LIB)
        if (NOT EXISTS "${PYARROW_INCLUDE_DIR}/arrow/api.h" OR NOT EXISTS "${PYARROW_ARROW_LIB}")
            message(FATAL_ERROR "Could not find Arrow headers/libs under pyarrow at ${PYARROW_ROOT}")
        endif()

        add_library(Arrow::arrow_shared SHARED IMPORTED)
        set_target_properties(Arrow::arrow_shared PROPERTIES
            IMPORTED_LOCATION "${PYARROW_ARROW_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${PYARROW_INCLUDE_DIR}"
        )
        set(ARROW_TARGET Arrow::arrow_shared)
        set(ARROW_RUNTIME_DIR "${PYARROW_ROOT}")
        message(STATUS "Arrow C++ found via pyarrow at ${PYARROW_ROOT}")
    endif()

    list(APPEND INGEST_SOURCES
        src/io/FeatherEventReader.cpp
        src/runners/FeatherHardRunnerSupport.cpp
    )
    list(APPEND INGEST_TEST_SOURCES
        tests/io/FeatherEventReaderTest.cpp
    )
endif()

add_library(ingest_core STATIC ${INGEST_SOURCES})
target_include_directories(ingest_core PUBLIC src)
target_include_directories(ingest_core PRIVATE third_party/simdjson)
target_link_libraries(ingest_core PUBLIC Threads::Threads)
if (ENABLE_ARROW)
    target_compile_definitions(ingest_core PUBLIC MD_ENABLE_ARROW=1)
    target_link_libraries(ingest_core PUBLIC ${ARROW_TARGET})
endif()

add_executable(ingest src/main.cpp)
target_link_libraries(ingest PRIVATE ingest_core)
if (ENABLE_ARROW AND DEFINED ARROW_RUNTIME_DIR)
    set_target_properties(ingest PROPERTIES BUILD_RPATH "${ARROW_RUNTIME_DIR}")
endif()

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
add_executable(ingest_tests ${INGEST_TEST_SOURCES})
target_link_libraries(ingest_tests PRIVATE ingest_core)
target_include_directories(ingest_tests PRIVATE tests)
target_compile_definitions(ingest_tests PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/test_data")
if (ENABLE_ARROW AND DEFINED ARROW_RUNTIME_DIR)
    set_target_properties(ingest_tests PROPERTIES BUILD_RPATH "${ARROW_RUNTIME_DIR}")
endif()
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
    PASS_REGULAR_EXPRESSION "Summary[\r\n]+total_messages_processed=2[\r\n]+chronological_violations=0[\r\n]+first_timestamp=[0-9]+[\r\n]+last_timestamp=[0-9]+[\r\n]+wall_clock_seconds=[0-9]+\\.[0-9]+[\r\n]+throughput_messages_per_second=[0-9]+\\.[0-9]+"
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
