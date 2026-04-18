set(TARGET back-tester-ingest)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    simdjson-static-lib
)
