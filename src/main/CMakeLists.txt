file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(FILTER SRC EXCLUDE REGEX "/main\\.cpp$|^main\\.cpp$")

set(TARGET back-tester-lib)
if(SRC)
    add_library(${TARGET} STATIC ${SRC})
    target_link_libraries(${TARGET} PUBLIC
        back-tester-common
    )
else()
    add_library(${TARGET} INTERFACE)
    target_link_libraries(${TARGET} INTERFACE
        back-tester-common
    )
endif()

# executable
set(TARGET back-tester)
add_executable(${TARGET} main.cpp)
target_link_libraries(${TARGET} PUBLIC back-tester-lib backtest-strategies)
