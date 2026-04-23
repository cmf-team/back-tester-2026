file(GLOB_RECURSE LIB_SRC CONFIGURE_DEPENDS parsers/*.cpp)

set(LIB_TARGET back-tester-data-ingestion)
add_library(${LIB_TARGET} STATIC ${LIB_SRC})
target_link_libraries(${LIB_TARGET} PUBLIC back-tester-common)

set(EXE_TARGET simple-loader)
add_executable(${EXE_TARGET} simple_loader_main.cpp)
target_link_libraries(${EXE_TARGET} PRIVATE ${LIB_TARGET})
