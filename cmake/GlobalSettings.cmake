###########################################################
# Global settings

# Custom modules, if any
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

option(BUILD_TESTS "Build tests" ON)

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
print_message("Options:            BUILD_TESTS=${BUILD_TESTS}")
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

# Include dir
include_directories(src)
