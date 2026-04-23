set(TARGET back-tester-common)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})
add_dependencies(${TARGET} GenerateVersion simdjson-static-lib)

target_link_libraries(${TARGET} PUBLIC simdjson-static-lib
)
