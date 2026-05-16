# Changes

Refactor + optimization pass on the C++20 NDJSON market-data ingestion pipeline.

## Homework 2 Hard Variant Summary

- Enabled LOB processor for flat/hierarchy hard modes.
- Added deterministic final LOB digest for strategy comparison.
- Added LOB benchmark mode.
- Added async snapshot/log writer for stateless work.
- Added Feather conversion script.
- Added JSON vs Feather ingestion benchmark.
- Optional: added Arrow C++ Feather producer.
- Optional: added sharded LOB worker mode.

## Homework 2 Standard LOB

- Added `LimitOrderBook` for per-instrument L2 aggregation from L3 events.
- Added `BookManager` for multi-instrument routing and `order_id -> instrument_id` lookup.
- Added `LobMarketDataEventProcessor` implementing `IMarketDataEventProcessor`.
- Added CLI flags:
  - `--lob`
  - `--snapshot-depth`
  - `--snapshot-interval-events`
  - `--max-snapshots`
- Standard mode can now reconstruct LOB state from a single real Databento/XEUR MBO JSON file.
- Flat and hierarchy modes can also run with `--lob` after their chronological merge; LOB updates remain sequential in the dispatcher.
- Event semantics are explicit:
  - `Add`, `Modify`, `Cancel`, and `Clear` mutate the book.
  - `Trade` and `Fill` are handled as non-mutating events for the current reconstruction model.
- Final output includes LOB snapshots, final per-instrument `resting_orders`, best bid/ask, chronological violations, wall-clock time, and throughput.
- Real-file smoke test:
  - messages: 2,232,542
  - lines read: 2,232,542
  - instruments: 6
  - chronological violations: 0
  - unresolved events: 0
  - throughput: ~1.91M msg/sec
- Added hard-mode synthetic LOB smoke fixture covering identical final books for flat and hierarchy and missing `instrument_id` resolution through `order_id`.
- Added `BookManager::stableStateDigest()` for deterministic flat/hierarchy LOB equivalence tests, including equal timestamp tie-breakers.
- Added `--benchmark <folder> --lob` to benchmark flat vs hierarchy with the LOB processor, snapshots disabled, unresolved-event counts, and matching final LOB digest fingerprints.
- Added immutable `BookManagerSnapshot` capture plus `AsyncSnapshotWriter` so snapshot formatting/writing can run on a worker thread while LOB updates remain sequential in the dispatcher.
- Added CLI snapshot writer flags:
  - `--async-snapshots`
  - `--snapshot-writer sync|async`
  - `--snapshot-output PATH`
- Added bonus sharded LOB workers via `--lob-workers N`; worker queues own disjoint `BookManager` subsets, while the dispatcher preserves per-instrument FIFO order and uses an `order_id -> instrument_id` router index for missing-instrument events.
- Improved hard-task benchmark reporting:
  - logging benchmark prints `Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,WallClockSeconds,ThroughputMessagesPerSecond`
  - LOB benchmark prints the same columns plus `LobDigest` under a `Benchmark LOB` section, using `lob-sharded-N` processor labels for sharded runs
  - `scripts/benchmark.sh <json-folder> [feather-folder]` now captures logging and LOB runs, and can append Arrow-enabled Feather rows for JSON-vs-Feather speed comparisons
- Added `scripts/convert_to_feather.py` for converting `.mbo.json`, `.jsonl`, and `.ndjson` L3 files to one Feather file per input file while preserving row order and the L3 fields used by the project.
- Real-folder Feather conversion smoke:
  - input JSON files: 22
  - converted Feather files: 22
  - total JSON lines: 19,869,718
  - total Feather rows: 19,869,718
  - first/last `ts_recv`, `action`, and `side` preserved per file
  - `null` prices preserved as Arrow nulls
- Added `scripts/benchmark_feather_read.py` to compare Python-side NDJSON `json.loads` scan/parse with `pyarrow.feather.read_table`.
- Real-folder Python read benchmark:
  - JSON rows: 19,869,718 in 55.574674s (~357,531.89 rows/s)
  - Feather rows: 19,869,718 in 1.515689s (~13,109,362.05 rows/s)
  - row counts matched
- Added optional Arrow C++ Feather producer behind `-DENABLE_ARROW=ON`:
  - `src/io/FeatherEventReader`
  - `src/runners/FeatherHardRunnerSupport`
  - CLI `--input-format json|feather` for flat, hierarchy, and benchmark modes
  - default build still works without Arrow and gives a clear runtime error if Feather input is requested
- Arrow-enabled synthetic tests confirm Feather rows map to `MarketDataEvent`, null prices map to `UNDEF`, and Feather flat/hierarchy LOB digests match JSON.
- Added an opt-in Arrow-enabled real-folder test (`MD_RUN_REAL_FEATHER_TEST=1`) for JSON-vs-Feather flat digest and Feather flat-vs-hierarchy digest equality.
- Real-folder Arrow-enabled LOB smoke:
  - JSON flat/hierarchy and Feather flat/hierarchy all processed 19,869,718 rows
  - chronological violations: 0
  - unresolved events: 0
  - matching LOB digest: `0xf068b976ad32134e`

## What changed

### Domain / parsing
- Removed dead diagnostic counters (`empty_lines`, `invalid_json_lines`, `invalid_messages`) and the `ParseStatus` enum. The contract is "input is always correct"; failure paths were unreachable.
- `parseMarketDataEventLine` now returns `MarketDataEvent` directly (was `std::optional<MarketDataEvent>`).
- Rewrote `src/parsing/JsonParser.cpp` against simdjson (vendored at `third_party/simdjson`). Highlights:
  - `thread_local simdjson::ondemand::parser` + `thread_local std::vector<char>` padded buffer (simdjson requires `SIMDJSON_PADDING` trailing bytes).
  - Price parsing keeps Databento's fixed-point `1e-9` contract for quoted decimal strings, numeric decimals, and `null` (`INT64_MAX`).
  - Howard-Hinnant `daysFromCivil` + manual time math replaced with `std::chrono::sys_days` + `year/month/day`.
  - File now ~140 lines (was 273).

### I/O
- Added `src/io/MmapFile.{hpp,cpp}` (mmap reader; `nextLine()` returns `std::string_view` over the mapped bytes; `MADV_SEQUENTIAL` + `memchr` for line scan).
- `StandardRunner` and the producer threads in `HardRunnerSupport` now use `MmapFile` instead of `std::getline` on `std::ifstream`.

### Build
- `CMakeLists.txt`: added `src/io/MmapFile.cpp` and `third_party/simdjson/simdjson.cpp` to `ingest_core`. simdjson sources are built with `-Wno-error -w` (we don't own its warnings).
- `target_include_directories(ingest_core PRIVATE third_party/simdjson)`.

### Merge tuning
- Replaced the bounded queue stage with `NonBlockingQueue`, an unbounded batched SPSC queue. Producers publish batches without waiting for free ring slots; consumers wait for the next batch with `std::atomic::wait/notify_one`, not mutexes or condition variables.
- `event_queue_batch_size = 256` in `src/runners/HardRunnerSupport.hpp`.
- `mergeInputQueues` kept as a binary min-heap (`std::priority_queue<HeapNode>`). Considered a flat linear scan over `k` heads — for `k ≈ 20` the asymptotic difference (`log k` vs `k`) is small and the heap reads cleanly; left as-is.

### Profiling
- `build_profiling.sh` — release+debug build (`-O3 -g -fno-omit-frame-pointer -DNDEBUG`).
- `profile_standard.sh` — runs the binary under macOS `/usr/bin/sample` and `samply` (Firefox-Profiler flamegraph).
- `scripts/benchmark.sh` — benchmark driver.

## Performance

Standard runner, single file. After mmap migration:
- Total profile samples: **1432 → 528** (~2.7× faster).
- Old hot leaves gone: `string::push_back` (450), `getline` (284), `__read_nocancel` (68).
- New top leaf: `_platform_memchr` (~221) — expected; that's the line scanner over mmap'd bytes.

## How to use

### Build
```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run
Long-form:
```sh
./build/ingest --mode standard     --input <file.jsonl>   [--verbose] [--print-events N]
./build/ingest --mode flat         --input <folder>       [--verbose] [--print-events N]
./build/ingest --mode hierarchy    --input <folder>       [--verbose] [--print-events N]
./build/ingest --benchmark <folder> [--lob]
./build/ingest --mode flat --input <folder> --lob --async-snapshots --snapshot-output logs/lob_snapshots.txt
```

Shortcuts (mode as positional, then path):
```sh
./build/ingest <file.jsonl>                 # implicit standard
./build/ingest standard <file.jsonl>
./build/ingest flat <folder>
./build/ingest hierarchy <folder>
./build/ingest benchmark <folder>
```

`--print-events N` caps how many parsed events the dispatcher prints (default 10). `--benchmark` forces it to 0 so timing isn't bound by stdout. `scripts/benchmark.sh <folder>` prints both the logging benchmark and the LOB benchmark.

### Modes
- **standard** — single file, single thread, `MmapFile → parse → dispatch`.
- **flat** — folder of files; one producer thread per file feeds a batched SPSC queue, one merger thread runs a k-way min-heap merge across all queues, one dispatcher consumes the merged stream.
- **hierarchy** — folder of files; 4-way tree of mergers (parallelizable), useful when the merger itself is the bottleneck.
- **benchmark** — runs the hard-task pipeline silently for timing.

### Tests
```sh
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Profiling (macOS)
```sh
./build_profiling.sh                  # produces build-prof/ingest with -O3 -g
./profile_standard.sh <file.jsonl>    # writes a /usr/bin/sample text report and a samply .json.gz flamegraph
```
Open the `.json.gz` at <https://profiler.firefox.com> for the flamegraph.

## Layout
```
src/
  app/            CLI parsing
  domain/         MarketDataEvent + comparator
  io/             MmapFile
  parsing/        JsonParser (simdjson)
  processing/     IMarketDataEventProcessor + LoggingMarketDataEventProcessor
  runners/        StandardRunner, FlatMergeRunner, HierarchicalMergeRunner, HardRunnerSupport, ResultPrinter
  concurrency/    NonBlockingQueue
tests/            test_main.cpp (CTest target ingest_tests)
third_party/
  simdjson/       vendored simdjson.{h,cpp}
docs/             requirements
```
