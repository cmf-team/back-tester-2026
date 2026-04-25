file(GLOB LIB_SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/lob_main.cpp)

set(LIB_TARGET back-tester-lob)
add_library(${LIB_TARGET} STATIC ${LIB_SRC})
target_link_libraries(${LIB_TARGET} PUBLIC back-tester-data-ingestion)

set(EXE_TARGET lob-main)
add_executable(${EXE_TARGET} lob_main.cpp)
target_link_libraries(${EXE_TARGET} PRIVATE ${LIB_TARGET})
