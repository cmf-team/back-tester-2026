# CSV replay + Avellaneda–Stoikov strategies (linked by back-tester main).
set(TARGET backtest-strategies)

add_library(${TARGET} STATIC
    csv_data_loader.cpp
    strategy_avellaneda.cpp
    strategy_microprice.cpp
)

target_include_directories(${TARGET} PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
