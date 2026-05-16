#include "app/ArgsParser.hpp"
#include "backtest/BacktestReport.hpp"
#include "processing/BacktestMarketDataEventProcessor.hpp"
#include "processing/LoggingMarketDataEventProcessor.hpp"
#include "processing/LobMarketDataEventProcessor.hpp"
#include "processing/ShardedLobMarketDataEventProcessor.hpp"
#include "runners/BenchmarkRunner.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"
#include "runners/ResultPrinter.hpp"
#include "runners/StandardRunner.hpp"
#include "strategies/AvellanedaStoikovStrategy.hpp"
#include "strategies/FixedQuoteStrategy.hpp"
#include "strategies/MicropriceAvellanedaStoikovStrategy.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace md;

namespace {

RunResult runConfiguredMode(const AppConfig& config, IMarketDataEventProcessor& processor) {
    switch (config.mode) {
        case RunMode::Standard:
            return StandardRunner{}.run(config.input_path, processor, config.verbose, std::cerr);
        case RunMode::Flat:
            return FlatMergeRunner{}.run(
                config.input_path,
                processor,
                config.verbose,
                std::cerr,
                config.input_format
            );
        case RunMode::Hierarchy:
            return HierarchicalMergeRunner{}.run(
                config.input_path,
                processor,
                config.verbose,
                std::cerr,
                config.input_format
            );
        case RunMode::Benchmark:
        case RunMode::Help:
            break;
    }

    throw std::runtime_error("unsupported run mode");
}

FixedQuoteStrategyConfig fixedQuoteConfig(const AppConfig& config) {
    return FixedQuoteStrategyConfig{
        .instrument_id = config.backtest_instrument_id,
        .order_size = config.order_size,
        .quote_offset_ticks = config.quote_offset_ticks,
        .tick_size = config.tick_size,
        .quote_interval_events = config.quote_interval_events,
        .max_inventory = config.max_inventory
    };
}

AvellanedaStoikovConfig avellanedaStoikovConfig(const AppConfig& config) {
    return AvellanedaStoikovConfig{
        .instrument_id = config.backtest_instrument_id,
        .order_size = config.order_size,
        .gamma = config.gamma,
        .sigma = config.sigma,
        .k = config.k,
        .horizon_seconds = config.horizon_seconds,
        .tick_size = config.tick_size,
        .max_inventory = config.max_inventory,
        .quote_interval_events = config.quote_interval_events
    };
}

MicropriceAvellanedaStoikovConfig micropriceAvellanedaStoikovConfig(const AppConfig& config) {
    return MicropriceAvellanedaStoikovConfig{
        .instrument_id = config.backtest_instrument_id,
        .order_size = config.order_size,
        .gamma = config.gamma,
        .sigma = config.sigma,
        .k = config.k,
        .horizon_seconds = config.horizon_seconds,
        .tick_size = config.tick_size,
        .max_inventory = config.max_inventory,
        .quote_interval_events = config.quote_interval_events,
        .use_imbalance_skew = config.use_imbalance_skew,
        .imbalance_alpha_ticks = config.imbalance_alpha_ticks
    };
}

std::unique_ptr<Strategy> makeBacktestStrategy(const AppConfig& config) {
    if (config.strategy_name == "fixed_quote") {
        return std::make_unique<FixedQuoteStrategy>(fixedQuoteConfig(config));
    }
    if (config.strategy_name == "avellaneda_stoikov") {
        return std::make_unique<AvellanedaStoikovStrategy>(avellanedaStoikovConfig(config));
    }
    if (config.strategy_name == "microprice_avellaneda_stoikov") {
        return std::make_unique<MicropriceAvellanedaStoikovStrategy>(
            micropriceAvellanedaStoikovConfig(config)
        );
    }

    throw std::runtime_error("unknown backtest strategy: " + config.strategy_name);
}

BacktestReport makeBacktestReport(
    const BacktestMarketDataEventProcessor& processor,
    const AppConfig& config,
    const RunResult& result
) {
    return md::makeBacktestReport(BacktestReportInput{
        .strategy_name = config.strategy_name,
        .instrument_id = config.backtest_instrument_id,
        .events_processed = result.summary.total_messages_processed,
        .orders_placed = processor.orders().totalPlaced(),
        .orders_cancelled = processor.orders().totalCancelled(),
        .metrics = processor.metrics().current(),
        .max_inventory = processor.metrics().maxInventory(),
        .average_inventory = processor.metrics().averageInventory(),
        .wall_clock_seconds = result.wall_clock_seconds
    });
}

} // namespace

int main(int argc, char* argv[]) {
    const std::string executable_name = argc > 0 && argv[0] != nullptr ? argv[0] : "ingest";

    try {
        const AppConfig config = ArgsParser::parse(argc, argv);

        if (config.mode == RunMode::Help) {
            std::cout << ArgsParser::usage(executable_name);
            return 0;
        }

        if (config.mode == RunMode::Benchmark) {
            if (config.use_lob_processor) {
                printLobBenchmarkResults(
                    runLobBenchmark(
                        config.input_path,
                        config.input_format,
                        config.verbose,
                        std::cerr,
                        config.lob_workers
                    ),
                    std::cout
                );
                return 0;
            }

            printBenchmarkResults(
                runLoggingBenchmark(config.input_path, config.input_format, config.verbose, std::cerr),
                std::cout
            );
            return 0;
        }

        if (config.backtest_enabled) {
            auto strategy = makeBacktestStrategy(config);
            BacktestMarketDataEventProcessor processor{*strategy};
            const RunResult result = runConfiguredMode(config, processor);
            printBacktestReport(makeBacktestReport(processor, config, result), std::cout);
            printRunResult(result, std::cout, config.verbose, 0);
            return 0;
        }

        if (config.use_lob_processor) {
            const LobProcessorConfig lob_config{
                .snapshot_depth = config.snapshot_depth,
                .snapshot_interval_events = config.snapshot_interval_events,
                .max_snapshots = config.max_snapshots,
                .snapshot_writer_mode = config.snapshot_writer_mode,
            };

            std::ofstream snapshot_file;
            std::ostream* snapshot_out = &std::cout;
            if (!config.snapshot_output_path.empty()) {
                const auto parent_path = config.snapshot_output_path.parent_path();
                if (!parent_path.empty()) {
                    std::filesystem::create_directories(parent_path);
                }
                snapshot_file.open(config.snapshot_output_path);
                if (!snapshot_file.is_open()) {
                    throw std::runtime_error(
                        "cannot open snapshot output: " + config.snapshot_output_path.string()
                    );
                }
                snapshot_out = &snapshot_file;
            }

            if (config.lob_workers > 1) {
                ShardedLobMarketDataEventProcessor processor(
                    std::cout,
                    *snapshot_out,
                    config.lob_workers,
                    lob_config
                );
                const RunResult result = runConfiguredMode(config, processor);
                processor.finish();
                processor.printFinalSummary(std::cout);
                printRunResult(result, std::cout, config.verbose, 0);
            } else {
                LobMarketDataEventProcessor processor(std::cout, *snapshot_out, lob_config);
                const RunResult result = runConfiguredMode(config, processor);
                processor.finishSnapshots();
                processor.printFinalSummary(std::cout);
                printRunResult(result, std::cout, config.verbose, 0);
            }
            return 0;
        }

        LoggingMarketDataEventProcessor processor(std::cout, config.max_events_to_print);
        const RunResult result = runConfiguredMode(config, processor);
        printRunResult(result, std::cout, config.verbose, config.max_events_to_print);
        return 0;
    } catch (const ArgsError& e) {
        std::cerr << "Error: " << e.what() << "\n\n" << ArgsParser::usage(executable_name);
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
