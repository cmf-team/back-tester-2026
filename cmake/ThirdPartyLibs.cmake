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

