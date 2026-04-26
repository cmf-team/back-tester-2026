set(TARGET back-tester-common)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})
add_dependencies(${TARGET} GenerateVersion)

target_include_directories(${TARGET} PUBLIC ${CMAKE_SOURCE_DIR}/3rdparty/nlohmann_json/include)

target_link_libraries(${TARGET} PUBLIC
)
