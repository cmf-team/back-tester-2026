# get version info string for single component given a name and a path
# ComponentVersionInfo is the resulting string - out parameter
macro(GetVersion Name Dir VersionInfo)
    execute_process(
        COMMAND git describe --dirty --always --abbrev=40 --tags
        WORKING_DIRECTORY ${Dir}
        OUTPUT_VARIABLE Version
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${Dir}
        OUTPUT_VARIABLE Hash
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND git log -1 --format=%ad --date=iso8601
        WORKING_DIRECTORY ${Dir}
        OUTPUT_VARIABLE Date
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND git log -1 --format=%D
        WORKING_DIRECTORY ${Dir}
        OUTPUT_VARIABLE Branch
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND git log -1 --format=%s
        WORKING_DIRECTORY ${Dir}
        OUTPUT_VARIABLE Msg
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(${VersionInfo} "\"${Name}\", \"${Version}\", \"${Hash}\", \"${Date}\", \"${Branch}\", \"${Msg}\"")
endmacro()

# get version strings joined for multiple components
macro(GetComponentVersions ComponentDirList VersionInfo)
    while (${ComponentDirList})
        list(POP_FRONT ${ComponentDirList} ComponentName)
        list(POP_FRONT ${ComponentDirList} ComponentPath)
        GetVersion(${ComponentName} ${ComponentPath} ComponentVersionInfo)
        string(APPEND ${VersionInfo} "    {${ComponentVersionInfo}},")
        if (${ComponentDirList})
            string(APPEND ${VersionInfo} "\n")
        endif()
    endwhile()
endmacro()

# get list of subdirectories in a given directory
macro(GetSubdirectories dir result)
    file(GLOB children RELATIVE ${dir} ${dir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${dir}/${child})
            list(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

##############################################################

# main component
list(APPEND COMPONENT_DIR_LIST "SRC" ${CMAKE_SOURCE_DIR})

# add all 3rdparty libs as components
GetSubdirectories(${CMAKE_SOURCE_DIR}/3rdparty libs)
foreach(lib ${libs})
    list(APPEND COMPONENT_DIR_LIST ${lib} ${CMAKE_SOURCE_DIR}/3rdparty/${lib})
    list(APPEND THIRD_PARTY_COMPONENTS ${lib})
endforeach()

print_message("Third party libs:   ${THIRD_PARTY_COMPONENTS}")
print_message("----------------------------------------")

# generate the version file: Version.hpp
GetComponentVersions(COMPONENT_DIR_LIST VERSION_INFO)
configure_file(${CMAKE_SOURCE_DIR}/src/Version.hpp.in ${CMAKE_BINARY_DIR}/include/Version.hpp)

# regenerate Version.hpp on the build step
add_custom_target(GenerateVersion
    COMMAND ${CMAKE_COMMAND} -DDISABLE_PRINT=1 .
    DEPENDS ${CMAKE_SOURCE_DIR}/src/Version.hpp.in ${THIRD_PARTY_COMPONENTS}
    COMMENT "Configuring Version.hpp"
)
