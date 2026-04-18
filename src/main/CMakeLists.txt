file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# static library
set(TARGET back-tester-lib)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-parser
)

# executable
set(TARGET back-tester)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC back-tester-lib)
