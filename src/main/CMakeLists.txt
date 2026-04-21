file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC main.cpp)

# static library
set(TARGET back-tester-lib)
add_library(${TARGET} STATIC ${SRC})
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
    back-tester-ingestion
)

# executable
set(TARGET back-tester)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC back-tester-lib back-tester-ingestion Threads::Threads)
