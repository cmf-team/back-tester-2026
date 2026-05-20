###########################################################
# Global settings

# Custom modules, if any
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" ON)

# Choose build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(ProcessorCount)
ProcessorCount(PROCESSOR_COUNT)

site_name(BUILD_NODE)

###########################################################
# Print info

set(DISABLE_PRINT_MESSAGE ${DISABLE_PRINT})
unset(DISABLE_PRINT CACHE)

macro(print_message)
    if(NOT DISABLE_PRINT_MESSAGE)
        message("${ARGV}")
    endif()
endmacro()

print_message("----------------------------------------")
print_message("Options:            BUILD_TESTS=${BUILD_TESTS} BUILD_BENCHMARKS=${BUILD_BENCHMARKS}")
print_message("Build type:         ${CMAKE_BUILD_TYPE}")
print_message("Build host:         ${BUILD_NODE}")
print_message("Processor count:    ${PROCESSOR_COUNT}")
print_message("Host OS:            ${CMAKE_HOST_SYSTEM}")
print_message("Target OS:          ${CMAKE_SYSTEM}")
print_message("Compiler:           ${CMAKE_CXX_COMPILER}")
print_message("Compiler id:        ${CMAKE_CXX_COMPILER_ID}, frontend: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}, \
version: ${CMAKE_CXX_COMPILER_VERSION}, launcher: ${CMAKE_CXX_COMPILER_LAUNCHER}")

###########################################################
# Compile settings

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    if(NOT CMAKE_OSX_SYSROOT)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _cmf_osx_sysroot
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY)
        set(CMAKE_OSX_SYSROOT "${_cmf_osx_sysroot}"
            CACHE PATH "macOS SDK for builds" FORCE)
    endif()

    # Older Apple Clang + newer SDK: libc++ headers need an explicit isystem.
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-isysroot${CMAKE_OSX_SYSROOT}>"
        "$<$<COMPILE_LANGUAGE:CXX>:-isystem${CMAKE_OSX_SYSROOT}/usr/include/c++/v1>"
        "$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>")
    add_link_options(-isysroot${CMAKE_OSX_SYSROOT} -stdlib=libc++)
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static libs only" FORCE)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Debug options
add_compile_options($<$<CONFIG:Debug>:-O0> $<$<CONFIG:Debug>:-gdwarf-4>)
add_compile_options($<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG>)

# Warnings
add_compile_options(-Werror -Wall -Wextra)
#add_compile_options(-Wfatal-errors -ftemplate-backtrace-limit=0)

# Architecture optimization
add_compile_options(-march=native)

# Include dir
include_directories(src)
