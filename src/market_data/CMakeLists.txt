set(TARGET back-tester-market-data)
file(GLOB SRC CONFIGURE_DEPENDS *.cpp)

# ArrowFeatherSource depends on libarrow; exclude it from the build unless
# the user opted in with -DENABLE_ARROW=ON.
if(NOT ENABLE_ARROW)
    list(REMOVE_ITEM SRC ${CMAKE_CURRENT_SOURCE_DIR}/ArrowFeatherSource.cpp)
endif()

add_library(${TARGET} STATIC ${SRC})

target_link_libraries(${TARGET} PUBLIC
    back-tester-common
)

if(ENABLE_ARROW)
    target_link_libraries(${TARGET} PUBLIC Arrow-static-lib)
endif()
