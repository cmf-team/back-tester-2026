#include "backtest/csv_data_loader.hpp"
#include "backtest/replay_engine.hpp"
#include "backtest/strategy_avellaneda.hpp"
#include "backtest/strategy_microprice.hpp"

#include <cstdio>
#include <cstring>
#include <exception>

int main(int argc, char** argv) {
    const char* csv_path = "MD/trades.csv";
    bool use_micro = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--micro") == 0) {
            use_micro = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::printf(
                "Usage: %s [path/to/trades.csv] [--micro]\n"
                "  Default CSV: MD/trades.csv (relative to current working directory)\n"
                "  --micro     Avellaneda–Stoikov with microprice reference\n",
                argv[0]);
            return 0;
        } else {
            csv_path = argv[i];
        }
    }

    try {
        backtest::CsvDataLoader loader(csv_path);
        const auto& events = loader.load();
        std::printf("Loaded %zu trade events\n", events.size());

        constexpr double speed_multiplier = 0.0;

        if (use_micro) {
            backtest::MicropriceAvellanedaStrategy strategy{};
            backtest::ReplayEngine engine(events, strategy, speed_multiplier);
            engine.run();
        } else {
            backtest::AvellanedaStoikovStrategy strategy{};
            backtest::ReplayEngine engine(events, strategy, speed_multiplier);
            engine.run();
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }

    return 0;
}
