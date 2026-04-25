file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# static library for non-main sources (if any — currently empty, reserved for
# future growth).
if(SRC)
    set(TARGET back-tester-lib)
    add_library(${TARGET} STATIC ${SRC})
    target_link_libraries(${TARGET} PUBLIC
        back-tester-common
        back-tester-market-data
    )
endif()

# executable
set(TARGET back-tester)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-market-data
    back-tester-order-book
    back-tester-dispatcher
)
if(TARGET back-tester-lib)
    target_link_libraries(back-tester PUBLIC back-tester-lib)
endif()
