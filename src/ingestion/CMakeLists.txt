file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

set(TARGET back-tester-ingestion)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC back-tester-common)
target_compile_definitions(${TARGET} PRIVATE _DEFAULT_SOURCE)