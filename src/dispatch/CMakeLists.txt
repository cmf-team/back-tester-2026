file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

set(TARGET back-tester-dispatch)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-lob
    back-tester-ingestion
)
