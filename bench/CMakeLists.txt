set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/bench)

file(GLOB BENCH_SRC CONFIGURE_DEPENDS *.cpp)

set(TARGET back-tester-bench)
add_executable(${TARGET} ${BENCH_SRC})

target_link_libraries(${TARGET} PRIVATE
    benchmark-static-lib
    back-tester-ingestion
    back-tester-common
)
