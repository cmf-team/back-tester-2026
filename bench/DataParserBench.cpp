#include "ingestion/NativeDataParser.hpp"
#include "ingestion/SimpleDataParser.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>

static const char *dataFilePath() {
  const char *env = std::getenv("BENCH_FILE");
  return env;
}

template <typename Parser>
static void BM_DataParser_Parse(benchmark::State &state) {
  const std::filesystem::path path = dataFilePath();
  if (!std::filesystem::exists(path)) {
    state.SkipWithError("Data file not found. Set BENCH_FILE env var.");
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

REGISTER_PARSER_BENCH(cmf::SimpleDataParser);
REGISTER_PARSER_BENCH(cmf::NativeDataParser);
