set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/benchmark)

file(GLOB BENCH_SRC CONFIGURE_DEPENDS *.cpp)

set(TARGET back-tester-benchmarks)
add_executable(${TARGET} ${BENCH_SRC})

target_link_libraries(${TARGET} PRIVATE
    benchmark-static-lib
    back-tester-common
    back-tester-parser
    back-tester-merge
)
