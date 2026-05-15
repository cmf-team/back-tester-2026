#include "app/ArgsParser.hpp"
#include "processing/LoggingMarketDataEventProcessor.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"
#include "runners/ResultPrinter.hpp"
#include "runners/StandardRunner.hpp"

#include <exception>
#include <iostream>
#include <vector>

using namespace md;

namespace {

RunResult runConfiguredMode(const AppConfig& config, IMarketDataEventProcessor& processor) {
    switch (config.mode) {
        case RunMode::Standard:
            return StandardRunner{}.run(config.input_path, processor, config.verbose, std::cerr);
        case RunMode::Flat:
            return FlatMergeRunner{}.run(config.input_path, processor, config.verbose, std::cerr);
        case RunMode::Hierarchy:
            return HierarchicalMergeRunner{}.run(config.input_path, processor, config.verbose, std::cerr);
        case RunMode::Benchmark:
        case RunMode::Help:
            break;
    }

    throw std::runtime_error("unsupported run mode");
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
            std::vector<RunResult> benchmark_results;
            benchmark_results.reserve(2);

            {
                LoggingMarketDataEventProcessor processor(std::cout, 0);
                benchmark_results.push_back(
                    FlatMergeRunner{}.run(config.input_path, processor, config.verbose, std::cerr)
                );
            }

            {
                LoggingMarketDataEventProcessor processor(std::cout, 0);
                benchmark_results.push_back(
                    HierarchicalMergeRunner{}.run(config.input_path, processor, config.verbose, std::cerr)
                );
            }

            printBenchmarkResults(benchmark_results, std::cout);
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
