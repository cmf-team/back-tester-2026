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

file(MAKE_DIRECTORY
    "${CMAKE_BINARY_DIR}/include"
    "${CMAKE_BINARY_DIR}/lib"
)

function(add_external_install_paths TARGET_NAME)
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC INTERFACE
        "${CMAKE_BINARY_DIR}/include"
    )
    target_link_directories(${TARGET_NAME} PUBLIC INTERFACE
        "${CMAKE_BINARY_DIR}/lib"
    )
endfunction()

function(configure_git_or_local_external)
    set(options)
    set(oneValueArgs TARGET_NAME SOURCE_DIR BINARY_DIR GIT_REPOSITORY GIT_TAG)
    set(multiValueArgs CMAKE_ARGS)
    cmake_parse_arguments(EXT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(EXISTS "${EXT_SOURCE_DIR}/CMakeLists.txt")
        ExternalProject_Add(
            ${EXT_TARGET_NAME}
            SOURCE_DIR "${EXT_SOURCE_DIR}"
            BINARY_DIR "${EXT_BINARY_DIR}"
            DOWNLOAD_COMMAND ""
            UPDATE_COMMAND ""
            CMAKE_ARGS ${EXT_CMAKE_ARGS}
            BUILD_COMMAND $(MAKE)
            INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
        )
    else()
        ExternalProject_Add(
            ${EXT_TARGET_NAME}
            GIT_REPOSITORY ${EXT_GIT_REPOSITORY}
            GIT_TAG ${EXT_GIT_TAG}
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
            SOURCE_DIR "${EXT_SOURCE_DIR}"
            BINARY_DIR "${EXT_BINARY_DIR}"
            CMAKE_ARGS ${EXT_CMAKE_ARGS}
            BUILD_COMMAND $(MAKE)
            INSTALL_COMMAND $(MAKE) -s DESTDIR=${DESTDIR} install
        )
    endif()
endfunction()

# ---------------------------------------------------------------------------------------
# Catch2 - C++ testing framework
configure_git_or_local_external(
    TARGET_NAME Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/Catch2"
    BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/Catch2"
    CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
)

set(TGT Catch2-static-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} Catch2)
add_external_install_paths(${TGT})
target_link_libraries(${TGT} PUBLIC INTERFACE
    $<$<CONFIG:Debug>:-lCatch2Maind -lCatch2d>
    $<$<CONFIG:Release>:-lCatch2Main -lCatch2>
)

# ---------------------------------------------------------------------------------------
# simdjson - fast JSON parser, used by the ingest layer
configure_git_or_local_external(
    TARGET_NAME simdjson
    GIT_REPOSITORY https://github.com/simdjson/simdjson.git
    GIT_TAG v3.10.1
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/3rdparty/simdjson"
    BINARY_DIR "${CMAKE_BINARY_DIR}/3rdparty/simdjson"
    CMAKE_ARGS ${FORWARDED_CMAKE_ARGS}
        -DSIMDJSON_DEVELOPER_MODE=OFF
        -DSIMDJSON_JUST_LIBRARY=ON
        -DBUILD_SHARED_LIBS=OFF
)

set(TGT simdjson-static-lib)
add_library(${TGT} INTERFACE)
add_dependencies(${TGT} simdjson)
add_external_install_paths(${TGT})
target_link_libraries(${TGT} PUBLIC INTERFACE -lsimdjson)
