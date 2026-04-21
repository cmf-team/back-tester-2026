file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC main.cpp)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

set(TARGET back-tester-lib)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC back-tester-common nlohmann_json::nlohmann_json)

set(TARGET back-tester)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC back-tester-lib)