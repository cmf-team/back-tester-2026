# Self-contained CSV replay + Avellaneda–Stoikov strategies (from shared hft-backtest-engine module).
set(TARGET as-backtest)

add_executable(${TARGET}
    as_main.cpp
    csv_data_loader.cpp
    strategy_avellaneda.cpp
    strategy_microprice.cpp
)

target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
