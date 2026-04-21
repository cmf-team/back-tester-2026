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
# zstd - fast lossless compression (used to read source L3 data files)
ExternalProject_Add(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG v1.5.6
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/zstd"
    BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/zstd"
    SOURCE_SUBDIR build/cmake
    CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
        -DZSTD_BUILD_PROGRAMS=OFF
        -DZSTD_BUILD_SHARED=OFF
        -DZSTD_BUILD_STATIC=ON
        -DZSTD_BUILD_TESTS=OFF
        -DZSTD_LEGACY_SUPPORT=OFF
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
)

set(TGT zstd-static-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} zstd)
target_include_directories(${TGT} SYSTEM PUBLIC INTERFACE ${CMAKE_BINARY_DIR}/include)
# Link the static archive explicitly (rather than '-lzstd') so the linker
# never silently picks up a shared libzstd.so from LD_LIBRARY_PATH / system
# paths. This keeps the resulting binary self-contained.
target_link_libraries(${TGT} PUBLIC INTERFACE
    ${CMAKE_BINARY_DIR}/lib/libzstd.a
)

