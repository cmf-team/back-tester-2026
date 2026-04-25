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
target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib ${CMAKE_BINARY_DIR}/lib64)
target_link_libraries(${TGT} PUBLIC INTERFACE
    $<$<CONFIG:Debug>:-lCatch2Maind -lCatch2d>
    $<$<CONFIG:Release>:-lCatch2Main -lCatch2>
)

# ---------------------------------------------------------------------------------------
# Apache Arrow C++ - direct .feather ingestion. Optional: opt in with
# `cmake -DENABLE_ARROW=ON`. The minimal feature set keeps the build under
# ~5 minutes the first time; subsequent builds reuse the source tree.
if(ENABLE_ARROW)
    message(STATUS "ENABLE_ARROW=ON - fetching apache-arrow")
    ExternalProject_Add(
        Arrow
        GIT_REPOSITORY https://github.com/apache/arrow.git
        GIT_TAG apache-arrow-18.0.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/arrow"
        SOURCE_SUBDIR cpp
        BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/arrow"
        CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
                   -DARROW_BUILD_STATIC=ON
                   -DARROW_BUILD_SHARED=OFF
                   -DARROW_IPC=ON
                   -DARROW_JSON=OFF
                   -DARROW_PARQUET=OFF
                   -DARROW_COMPUTE=OFF
                   -DARROW_DATASET=OFF
                   -DARROW_FILESYSTEM=OFF
                   -DARROW_TESTING=OFF
                   -DARROW_BUILD_TESTS=OFF
                   -DARROW_BUILD_EXAMPLES=OFF
                   -DARROW_BUILD_UTILITIES=OFF
                   -DARROW_BUILD_BENCHMARKS=OFF
                   -DARROW_DEPENDENCY_SOURCE=BUNDLED
                   -DARROW_WITH_LZ4=ON
                   -DARROW_WITH_ZSTD=ON
        BUILD_COMMAND $(MAKE)
        INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
    )

    set(TGT Arrow-static-lib)
    add_library(${TGT} INTERFACE)
    add_dependencies(${TGT} Arrow)
    target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
    target_link_directories(${TGT} PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/lib ${CMAKE_BINARY_DIR}/lib64)
    target_link_libraries(${TGT} PUBLIC INTERFACE arrow arrow_bundled_dependencies)
    target_compile_definitions(${TGT} PUBLIC INTERFACE BACKTESTER_HAS_ARROW=1)
endif()

