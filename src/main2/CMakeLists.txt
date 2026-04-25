file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

# Library variant of HW-2 driver, exposed for tests.
set(TARGET back-tester2-lib)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-parser
    back-tester-lob
    back-tester-pipeline
)

# Executable.
set(TARGET back-tester2)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC back-tester2-lib)
