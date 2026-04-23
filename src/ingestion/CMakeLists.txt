set(TARGET back-tester-ingestion)

file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_executable(${TARGET} ${SRC})

target_link_libraries(${TARGET}
    back-tester-common
)