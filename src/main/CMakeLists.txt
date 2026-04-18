file(GLOB SRC CONFIGURE_DEPENDS *.cpp)
list(REMOVE_ITEM SRC main.cpp)

# static library
set(TARGET back-tester-lib)
add_library(${TARGET} STATIC ${SRC}
        ../models/MarketDataEvent.h
        ../utils/MarketDataReader.h
        ../utils/MarketDataReader.cpp
        ../utils/MarketDataProcessor.cpp
        ../utils/MarketDataProcessor.h)
target_link_libraries(${TARGET} PUBLIC
    back-tester-common
)

# executable
set(TARGET back-tester)
add_executable(${TARGET} main.cpp
)
target_link_libraries(${TARGET} PUBLIC back-tester-lib)
