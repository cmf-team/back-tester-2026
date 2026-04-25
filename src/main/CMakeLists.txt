set(TARGET back-tester-lib)

add_library(${TARGET} STATIC
    DataIngestionLayer.cpp
    MarketDataEvent.cpp
    LimitOrderBook.cpp
    LobRouter.cpp
)

target_link_libraries(${TARGET} PUBLIC
    back-tester-common
)

set(TARGET back-tester)

add_executable(${TARGET}
    main.cpp
)

target_link_libraries(${TARGET} PUBLIC
    back-tester-lib
)