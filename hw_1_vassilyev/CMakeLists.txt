cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
project(hw1_market_data_ingestion LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

add_executable(hw1_ingestion main.cpp)
target_link_libraries(hw1_ingestion PRIVATE nlohmann_json::nlohmann_json)
