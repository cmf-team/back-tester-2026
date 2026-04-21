#include "ingestion/FeatherDataParser.hpp"
#include "ingestion/JsonNativeDataParser.hpp"
#include "ingestion/JsonSimpleDataParser.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>

template <typename Parser>
static void BM_DataParser_Parse(benchmark::State &state) {
  const char *env = std::getenv("BENCH_FILE");
  if (!env) {
    state.SkipWithError("BENCH_FILE env var not set.");
    return;
  }
  const std::filesystem::path path =
      std::string(env) + std::string(Parser::bench_suffix);
  if (!std::filesystem::exists(path)) {
    state.SkipWithError("Data file not found: " + path.string());
    return;
  }
  int64_t total_events = 0;
  for (auto _ : state) {
    Parser parser(path);
    int64_t count = 0;
    parser.parse([&](const cmf::MarketDataEvent &) { ++count; });
    benchmark::DoNotOptimize(count);
    total_events = count;
  }
  state.SetItemsProcessed(state.iterations() * total_events);
}

#define REGISTER_PARSER_BENCH(Type)                                            \
  BENCHMARK_TEMPLATE(BM_DataParser_Parse, Type)                                \
      ->Unit(benchmark::kMillisecond)                                          \
      ->Iterations(3)                                                          \
      ->Repetitions(3)                                                         \
      ->DisplayAggregatesOnly()

REGISTER_PARSER_BENCH(cmf::JsonSimpleDataParser);
REGISTER_PARSER_BENCH(cmf::JsonNativeDataParser);
REGISTER_PARSER_BENCH(cmf::FeatherDataParser);
