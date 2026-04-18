add_subdirectory(common)
add_subdirectory(main)

include(FetchContent)
FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG  v3.12.2
)
FetchContent_MakeAvailable(simdjson)

target_link_libraries(back-tester-lib PUBLIC simdjson::simdjson)