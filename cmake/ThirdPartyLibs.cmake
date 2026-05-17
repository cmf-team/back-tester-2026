include(ExternalProject)

# Passed to CMake-based libraries to support Intel compiler.
set(FORWARDED_CMAKE_ARGS
    --no-warn-unused-cli
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_INSTALL_MESSAGE=LAZY
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}
    -DCMAKE_INSTALL_LIBDIR=lib
)

set(DESTDIR "")

# ---------------------------------------------------------------------------------------
# Catch2 - C++ testing framework
ExternalProject_Add(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/Catch2"
    BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/Catch2"
    CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
)

set(TGT Catch2-static-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} Catch2)
target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib)
target_link_libraries(${TGT} PUBLIC INTERFACE
    $<$<CONFIG:Debug>:-lCatch2Maind -lCatch2d>
    $<$<CONFIG:Release>:-lCatch2Main -lCatch2>
)

# ---------------------------------------------------------------------------------------
# Google Benchmark
ExternalProject_Add(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.5
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/googlebenchmark"
    BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/googlebenchmark"
    CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
              -DBENCHMARK_ENABLE_TESTING=OFF
              -DBENCHMARK_ENABLE_GTEST_TESTS=OFF
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
)

set(TGT googlebenchmark-static-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} googlebenchmark)
target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib)
target_link_libraries(${TGT} PUBLIC INTERFACE -lbenchmark_main -lbenchmark)

# ---------------------------------------------------------------------------------------
# Apache Arrow C++ - enough for reading .feather files
ExternalProject_Add(
        apache-arrow
        GIT_REPOSITORY https://github.com/apache/arrow.git
        GIT_TAG apache-arrow-24.0.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/apache-arrow"
        BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/apache-arrow"
        SOURCE_SUBDIR cpp
        CMAKE_ARGS
        ${FORWARDED_CMAKE_ARGS}
        -DARROW_BUILD_SHARED=ON
        -DARROW_BUILD_STATIC=OFF
        -DARROW_BUILD_TESTS=OFF
        -DARROW_BUILD_BENCHMARKS=OFF
        -DARROW_BUILD_EXAMPLES=OFF
        -DARROW_BUILD_UTILITIES=OFF

        # Needed for Feather / IPC
        -DARROW_IPC=ON

        # Optional features you likely do not need just for Feather
        -DARROW_COMPUTE=OFF
        -DARROW_CSV=OFF
        -DARROW_JSON=OFF
        -DARROW_DATASET=OFF
        -DARROW_FILESYSTEM=OFF
        -DARROW_PARQUET=OFF

        # Compression support for typical Feather files
        -DARROW_WITH_ZLIB=ON
        -DARROW_WITH_LZ4=ON
        -DARROW_WITH_ZSTD=ON

        # Let Arrow build/fetch its own third-party deps
        -DARROW_DEPENDENCY_SOURCE=BUNDLED
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
)

set(TGT apache-arrow-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} apache-arrow)
target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib)
target_link_libraries(${TGT} PUBLIC INTERFACE -larrow)

# ---------------------------------------------------------------------------------------
# Abseil
ExternalProject_Add(
        abseil-cpp
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20260107.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/abseil-cpp"
        BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/abseil-cpp"
        CMAKE_ARGS
        ${FORWARDED_CMAKE_ARGS}
        -DABSL_BUILD_TESTING=OFF
        -DABSL_USE_GOOGLETEST_HEAD=OFF
        -DABSL_PROPAGATE_CXX_STD=ON
        -DCMAKE_CXX_STANDARD=20
        -DABSL_ENABLE_INSTALL=ON
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
)

set(TGT abseil-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} abseil-cpp)
target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib)

# Abseil static libs with their transitive dependencies.
# GNU ld (Linux) requires every transitive dep to appear on the link line;
# --start-group/--end-group lets the linker make multiple passes to resolve
# circular references between static archives.
set(_ABSL_LIBS
    ${CMAKE_BINARY_DIR}/lib/libabsl_raw_hash_set.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_hash.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_city.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_hashtablez_sampler.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_exponential_biased.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_base.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_raw_logging_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_spinlock_wait.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_malloc_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_throw_delegate.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_strings.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_strings_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_int128.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_synchronization.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_graphcycles_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_kernel_timeout_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_time.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_civil_time.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_time_zone.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_stacktrace.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_symbolize.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_debugging_internal.a
    ${CMAKE_BINARY_DIR}/lib/libabsl_demangle_internal.a
)

if(UNIX AND NOT APPLE)
    target_link_libraries(${TGT} PUBLIC INTERFACE
        -Wl,--start-group ${_ABSL_LIBS} -Wl,--end-group
    )
else()
    target_link_libraries(${TGT} PUBLIC INTERFACE ${_ABSL_LIBS})
endif()