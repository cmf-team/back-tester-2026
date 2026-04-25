set(TARGET back-tester-dispatcher)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

add_library(${TARGET} STATIC ${SRC})

target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-market-data
    back-tester-order-book
)
