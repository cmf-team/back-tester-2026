# Market Data Ingestion

A C++20 data-ingestion layer for an event-driven backtester. The project supports both the Standard task and the Hard task in one codebase:

- **Standard mode** reads one Databento NDJSON L3 file, creates `MarketDataEvent` objects, calls `processMarketDataEvent`, and prints the required summary plus the first/last 10 events.
- **Flat mode** reads a folder of chronologically sorted daily files with one producer thread per file and performs a single-level k-way merge.
- **Hierarchy mode** uses the same producers but merges streams through a binary tree of smaller mergers.
- **Benchmark mode** runs both Hard-task strategies and reports message count, wall-clock time, and throughput. With `--lob`, it benchmarks the same merge strategies while reconstructing final LOB state.

The default JSON build has no third-party runtime dependency. The parser is a small flat JSON-object parser tuned for NDJSON market-data rows, so reviewers do not need to install `nlohmann/json` or any package beyond a C++20 compiler and CMake. Arrow C++ is optional and used only when configuring with `-DENABLE_ARROW=ON`.

## Quick start

```bash
./run standard data/sample.ndjson
./run flat data/multi
./run hierarchy data/multi
./run benchmark data/multi
./run test
./scripts/benchmark.sh data/multi
```

The `run` script builds the project first and then executes the requested mode. `scripts/benchmark.sh <folder>` builds a release binary and prints report-ready logging and LOB benchmark blocks for that folder.

## Manual build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI

Backward-compatible Standard-task shortcut:

```bash
./build/ingest /path/to/day.ndjson
```

Explicit modes:

```bash
./build/ingest --mode standard --input /path/to/day.ndjson
./build/ingest --mode flat --input /path/to/folder
./build/ingest --mode hierarchy --input /path/to/folder
./build/ingest --benchmark /path/to/folder
./build/ingest --benchmark /path/to/folder --lob
```

Optional flags:

```bash
--verbose          Print parser diagnostics to stderr/stdout diagnostics section.
--print-events N   Print the first N events observed by processMarketDataEvent.
--lob              Reconstruct per-instrument LOBs in standard, flat, hierarchy, or benchmark mode.
--lob-workers N    Use N sharded LOB workers; default 1 keeps the dispatcher-owned sequential BookManager.
--snapshot-depth N Print N price levels per side in each LOB snapshot.
--snapshot-interval-events N
                   Print one LOB snapshot every N processed events.
--max-snapshots N  Cap the number of printed LOB snapshots.
--async-snapshots  Format and write LOB snapshots on a background worker.
--snapshot-writer sync|async
                   Select synchronous or asynchronous snapshot writing.
--snapshot-output PATH
                   Write LOB snapshots to PATH instead of stdout.
```

`benchmark` mode suppresses per-event logging by default because printing every event would dominate the measured runtime. `--benchmark ... --lob` also disables LOB snapshots by default so stdout is not part of the measured cost.

## Homework 2 Standard: LOB Reconstruction

Run:

```bash
./build/ingest --mode standard \
  --input data/XEUR-20260409-HTT6HHLT6R/xeur-eobi-20260309.mbo.json \
  --lob \
  --snapshot-depth 5 \
  --snapshot-interval-events 500000 \
  --max-snapshots 4 \
  --verbose
```

This mode reads one L3 NDJSON file, creates `MarketDataEvent` objects, routes them sequentially into `BookManager`, and maintains one `LimitOrderBook` per `instrument_id`.

Event semantics:

```text
Add / Modify / Cancel / Clear mutate the book.
Trade / Fill are explicitly handled as non-mutating events for the current reconstruction model.
```

Final `best_bid` / `best_ask` can be `<none>` when the file ends after exchange clear/reset events. Snapshots above the final summary demonstrate non-empty intraday book states.

Validated real-file smoke result on `data/XEUR-20260409-HTT6HHLT6R/xeur-eobi-20260309.mbo.json`:

```text
total_messages_processed=2232542
total_lines_read=2232542
instrument_count=6
chronological_violations=0
unresolved_events=0
wall_clock_seconds=1.169603
throughput_messages_per_second=1908803.703721
```

The final output includes:

```text
LOB Snapshot
...

Final LOB Summary
instrument_count=<N>
processed_events=<N>
unresolved_events=<N>
instrument_id=<id> resting_orders=<N> best_bid=<price|<none>> best_ask=<price|<none>>
...

Summary
total_messages_processed=<N>
chronological_violations=0
first_timestamp=<...>
last_timestamp=<...>
wall_clock_seconds=<positive>
throughput_messages_per_second=<positive>
Diagnostics
total_lines_read=<N>
```

For the hard-task ingestion modes, the same LOB processor can run after the chronological merge:

```bash
./build/ingest --mode flat --input tests/test_data/hard_lob_synthetic \
  --lob --snapshot-interval-events 3 --max-snapshots 2 --snapshot-depth 5 --verbose

./build/ingest --mode hierarchy --input tests/test_data/hard_lob_synthetic \
  --lob --snapshot-interval-events 3 --max-snapshots 2 --snapshot-depth 5 --verbose
```

Both modes dispatch the merged stream sequentially, so LOB updates remain deterministic. The synthetic hard LOB fixture verifies that a cancel with `instrument_id=0` is routed through the `order_id -> instrument_id` mapping.

Flat and hierarchy LOB equivalence is tested automatically with `BookManager::stableStateDigest()`. The digest sorts instruments by `instrument_id`, bids descending, asks ascending, and includes processed/unresolved counts, best bid/ask, resting order counts, and all L2 price levels. Equal timestamps are deterministic because the merge comparator uses `(timestamp, source_file_id, source_sequence)`.

Snapshot writing can be parallelized safely:

```bash
./build/ingest --mode flat \
  --input data/XEUR-20260409-HTT6HHLT6R \
  --lob \
  --snapshot-depth 5 \
  --snapshot-interval-events 1000000 \
  --max-snapshots 3 \
  --async-snapshots \
  --snapshot-output logs/lob_snapshots.txt \
  --verbose
```

The dispatcher still reads events in strict order and updates `BookManager` sequentially. When a snapshot is due, it copies immutable `BookManagerSnapshot` value data and submits that job to `AsyncSnapshotWriter`; the worker thread only formats/writes the copied data and has no access to mutable books.

### Sharded LOB Workers

`--lob-workers 2` and `--lob-workers 4` keep the global merge/dispatcher order, but route each resolved instrument to a fixed worker queue. Each worker owns its `BookManager` subset and updates its books sequentially, so per-instrument FIFO order is preserved.

The dispatcher keeps a lightweight `order_id -> instrument_id` router index for events whose `instrument_id` is missing. `Add` records the mapping, `Modify` keeps or updates it, full `Cancel` and `Clear` remove it, and unknown missing-instrument events are counted as unresolved.

For performance comparison:

```bash
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob --lob-workers 1
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob --lob-workers 2
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob --lob-workers 4
```

Sharded benchmark rows use `Processor=lob-sharded-2` or `Processor=lob-sharded-4` and still include throughput plus the final LOB digest.

## Homework 2 Hard Variant

The Hard variant runs the same LOB processor after the globally merged flat or hierarchy stream. By default, LOB updates are sequential and dispatcher-owned: producers and mergers only create an ordered event stream, then one dispatcher calls the processor in final timestamp order. Bonus sharding is enabled only when `--lob-workers N` is greater than `1`.

Flat and hierarchy differ only in how they merge sorted file streams. Flat uses one k-way heap over all file queues. Hierarchy builds a 4-way tree of smaller merger stages, which can spread merge work across more threads while preserving the same final order contract: `(timestamp, source_file_id, source_sequence)`. The final LOB digest is the strategy comparison key; matching digests mean both merge strategies reconstructed the same final book state.

JSON hard LOB commands:

```bash
./build/ingest --mode flat \
  --input data/XEUR-20260409-HTT6HHLT6R \
  --lob \
  --verbose

./build/ingest --mode hierarchy \
  --input data/XEUR-20260409-HTT6HHLT6R \
  --lob \
  --verbose
```

JSON benchmark command:

```bash
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob
```

Feather conversion command:

```bash
python3 scripts/convert_to_feather.py \
  --input data/XEUR-20260409-HTT6HHLT6R \
  --output data_feather/XEUR-20260409-HTT6HHLT6R
```

Python JSON vs Feather read benchmark:

```bash
python3 scripts/benchmark_feather_read.py \
  --json-input data/XEUR-20260409-HTT6HHLT6R \
  --feather-input data_feather/XEUR-20260409-HTT6HHLT6R
```

Optional C++ Arrow Feather producer:

```bash
cmake -S . -B build-arrow -DCMAKE_BUILD_TYPE=Release -DENABLE_ARROW=ON
cmake --build build-arrow -j

./build-arrow/ingest --mode flat \
  --input data_feather/XEUR-20260409-HTT6HHLT6R \
  --input-format feather \
  --lob \
  --verbose

./build-arrow/ingest --mode hierarchy \
  --input data_feather/XEUR-20260409-HTT6HHLT6R \
  --input-format feather \
  --lob \
  --verbose
```

Combined C++ JSON vs Feather benchmark:

```bash
./scripts/benchmark.sh \
  data/XEUR-20260409-HTT6HHLT6R \
  data_feather/XEUR-20260409-HTT6HHLT6R
```

JSON input is parsed from NDJSON text into `MarketDataEvent` objects. Feather input reads the same columns from Arrow/Feather files and maps each row to the same event contract, including `ts_recv`/`ts_event` timestamp fallback, fixed-point prices, null price handling, side/action enums, and source row metadata. Matching LOB digests across JSON and Feather confirm that the faster columnar path is behaviorally equivalent for final book reconstruction.

Optional sharded LOB commands:

```bash
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob --lob-workers 2
./build/ingest --benchmark data/XEUR-20260409-HTT6HHLT6R --lob --lob-workers 4
```

Snapshot/log output is stateless async work. The async writer receives immutable snapshot data and only formats/writes copied state; it does not mutate books or change event order. LOB reconstruction remains sequential unless `--lob-workers` is explicitly enabled, in which case the dispatcher routes each resolved instrument to one worker FIFO queue and each worker updates its own `BookManager` subset sequentially.

Core Hard task readiness:

```text
[PASS] ./build/ingest --mode flat --input <folder> --lob
[PASS] ./build/ingest --mode hierarchy --input <folder> --lob
[PASS] flat/hierarchy total_messages_processed equal
[PASS] flat/hierarchy chronological_violations == 0
[PASS] flat/hierarchy unresolved_events == 0 on the validated dataset
[PASS] flat/hierarchy final LOB digest equal
[PASS] benchmark --lob compares flat and hierarchy
[PASS] async stateless snapshot/log worker exists
[PASS] convert_to_feather.py exists
[PASS] JSON vs Feather ingestion speed comparison exists
[PASS] README documents all commands and results
```

Roadmap followed:

```text
H0. Baseline: current tests + Standard LOB + hard benchmark without LOB
H1. Enable --lob for flat/hierarchy
H2. Add flat vs hierarchy final LOB digest comparison
H3. Real-folder JSON smoke for flat/hierarchy + LOB
H4. Add --benchmark --lob
H5. Add async stateless snapshot/log writer
H6. Improve benchmark/report output
H7. Add convert_to_feather.py
H8. Add Python JSON vs Feather read benchmark
H9. Optional: C++ Arrow Feather producer
H10. Bonus: sharded LOB workers
H11. README/CHANGES/report cleanup
```

## Output

Standard mode keeps normal output close to the assignment requirements:

```text
timestamp=1773042761368148840, order_id=0, side=N, price=9223372036854775807, size=0, action=R
timestamp=1773042761368148840, order_id=10996414798222631105, side=B, price=21200000, size=20, action=A
...
Summary
total_messages_processed=1305607
first_timestamp=1773042761368148840
last_timestamp=1773079202630298007
First 10 MarketDataEvent objects
MarketDataEvent{timestamp=1773042761368148840, ts_recv=1773042761368148840, ts_event=1773042761367824437, order_id=0, side=N, price=9223372036854775807, size=0, action=R, instrument_id=34513, source_file_id=0, source_sequence=1}
...
Last 10 MarketDataEvent objects
...
MarketDataEvent{timestamp=1773079202630298007, ts_recv=1773079202630298007, ts_event=1773079202630075425, order_id=1773079200877309227, side=A, price=25370000, size=20, action=C, instrument_id=34600, source_file_id=0, source_sequence=1305607}
```

Diagnostics such as invalid JSON lines are not printed in normal mode. They are counted internally and shown only with `--verbose`.

Benchmark output is CSV-like:

```text
Benchmark
Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,WallClockSeconds,ThroughputMessagesPerSecond
flat,json,logging,15175617,0,0,0.934796,16234154.21
hierarchy,json,logging,15175617,0,0,0.955811,15876899.51
```

LOB benchmark output includes the processor type, unresolved order-routed events, and a stable fingerprint of the final `BookManager::stableStateDigest()`:

```text
Benchmark LOB
Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,WallClockSeconds,ThroughputMessagesPerSecond,LobDigest
flat,json,lob,15175617,0,0,1.234567,12288736.42,0x0123456789abcdef
hierarchy,json,lob,15175617,0,0,1.456789,10417183.36,0x0123456789abcdef
```

For report generation, `scripts/benchmark.sh <json-folder> [feather-folder]` builds Release and prints both blocks back to back: first logging-only ingestion/merge speed, then ingestion/merge plus LOB reconstruction. When a Feather folder is provided, it uses the Arrow-enabled build and appends the Feather rows to the same CSV-style blocks.

## Benchmark results

Benchmark environment:
- OS: macOS 15.6 (Darwin 24.6.0 arm64)
- CPU: Apple M4 Max, 16 logical CPUs (12 performance + 4 efficiency cores)
- Build: Release, -O3
- Input: 22 local Databento .mbo.json NDJSON files (4.9 GB) for Hard-task modes; Standard measured on a single 1.3 GB merged NDJSON file
- Event printing: disabled
- Data location: local APFS filesystem

Final benchmark table:

| Mode | InputFormat | Processor | Messages | Violations | Unresolved | Seconds | Throughput |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| standard | json | lob | 2,232,542 | 0 | 0 | 1.169603 | 1,908,803.70 |
| flat | json | lob | 19,869,718 | 0 | 0 | 5.220619 | 3,806,007.95 |
| hierarchy | json | lob | 19,869,718 | 0 | 0 | 5.868775 | 3,385,667.17 |
| flat | feather | lob | 19,869,718 | 0 | 0 | 4.012949 | 4,951,400.22 |
| hierarchy | feather | lob | 19,869,718 | 0 | 0 | 4.159643 | 4,776,784.87 |
| flat | json | lob-sharded-2 | 19,869,718 | 0 | 0 | 5.416377 | 3,668,451.50 |
| flat | json | lob-sharded-4 | 19,869,718 | 0 | 0 | 5.358407 | 3,708,139.05 |

All benchmark rows above have `chronological_violations=0`. The hard-folder LOB rows share the same final digest, `0xf068b976ad32134e`, across JSON flat/hierarchy, Feather flat/hierarchy, and sharded JSON runs.

CPU / thread utilization was measured separately with `samply`, so use this table for concurrency behavior rather than direct throughput comparison:

| Mode | Wall (ms) | Threads | CPU (ms) | Effective cores | M4 Max usage |
| --- | ---: | ---: | ---: | ---: | ---: |
| standard | 2687.6 | 1 | 2684 | 1.00 | 6% |
| flat | 1264.9 | 24 | 4110 | 3.25 | 20% |
| hierarchy | 925.7 | 32 | 5572 | 6.02 | 38% |

Standard mode uses one thread by design and keeps that thread saturated. Flat mode starts 22 producers, one merger, and one dispatcher; the merger/dispatcher pair dominates the sampled CPU time, leaving the single merger as the main funnel. Hierarchy mode starts the same producers plus 9 merger threads and one dispatcher, spreading work across more cores and reducing sampled wall time from about 1.26 s to 0.93 s.

The remaining headroom is mostly synchronization overhead at SPSC queue boundaries, visible as `__ulock_wait` in profiling, rather than lack of available CPU cores.

## Project structure

```text
src/
  app/          CLI configuration and argument parsing
  concurrency/  Queue primitives used by the merge pipeline
  domain/       Market-data event model
  io/           File-reading helpers
  parsing/      NDJSON / Databento row parsing
  processing/   Event processor interface and default logger
  runners/      Standard, flat-merge, hierarchy-merge, and benchmark runners
  main.cpp      Program entry point
tests/          Lightweight C++ tests split by scenario
data/           Small sample inputs for local checks and README commands
scripts/        Benchmark helper scripts
third_party/    Vendored build-time dependencies
```

## Architecture

`main.cpp` is intentionally small. It parses CLI arguments, selects a runner, creates an event processor, and prints the result. The ingestion logic lives in separate runners:

- `StandardRunner` reads a single NDJSON file line by line.
- `FlatMergeRunner` starts one producer per file, merges all stream heads with one priority queue, and dispatches the final stream.
- `HierarchicalMergeRunner` starts the same producers, then builds a binary tree of merger threads until one final stream remains.

The event consumer is behind an interface:

```cpp
class IMarketDataEventProcessor {
public:
    virtual ~IMarketDataEventProcessor() = default;
    virtual void processMarketDataEvent(const MarketDataEvent& event) = 0;
};
```

The default implementation is `LoggingMarketDataEventProcessor`. Homework 2 Standard uses `LobMarketDataEventProcessor`, which updates `BookManager` and reconstructs per-instrument L2 books without changing the ingestion runner.

## Chronological order guarantee

Every input file is assumed to be internally sorted by the Databento index timestamp. The global merge comparator uses:

```text
(timestamp, source_file_id, source_sequence)
```

This preserves chronological order and gives deterministic ordering when two files contain equal timestamps.

The dispatcher records `chronological_violations` while observing the final output stream. For a correct run this value must be `0`.

## Threading model

Hard-task modes use:

- one producer thread per input file;
- unbounded batched SPSC queues between pipeline stages;
- a single dispatcher thread that calls `processMarketDataEvent` sequentially.

Producer threads only read and parse. They never call the event processor. This keeps event handling single-threaded and preserves final stream order.

## Merge strategies

### Flat merger

The flat strategy keeps the next pending event from each input stream in one min-heap. Each pop emits the earliest event, then the next event from the same stream is pulled into the heap.

Complexity per emitted event:

```text
O(log k), where k is the number of input files
```

For roughly 20 files this is simple, deterministic, and efficient.

### Hierarchical merger

The hierarchy strategy builds a 4-way tree of mergers:

```text
file1 + file2 + file3 + file4 -> stream1234
file5 + file6 + file7 + file8 -> stream5678
stream1234 + stream5678 -> final stream
```

This uses multiple smaller priority queues and allows independent merge levels to run in parallel. A 4-way fan-in reduces intermediate queue hops while preserving deterministic ordering.

## Parser and Databento conventions

`MarketDataEvent::timestamp` uses Databento's index timestamp convention:

```text
ts_recv if present and not UINT64_MAX, otherwise ts_event
```

Prices are stored as `int64_t` fixed-precision values where one unit equals `1e-9`. A JSON `null` price maps to `INT64_MAX`, matching Databento's `UNDEF_PRICE` convention.

The parser accepts flat NDJSON JSON objects with numeric or string numeric fields. It supports integer prices, decimal prices, string prices, and `null` prices.

## Tests

The project uses lightweight C++ test executables instead of adding a third-party testing framework. Each test scenario lives in its own source file and is registered as a separate CTest case. Covered areas include:

- parser happy path and invalid input;
- timestamp fallback from `ts_recv` to `ts_event`;
- `null` and decimal price handling;
- CLI parsing and input validation;
- Standard runner diagnostics;
- LimitOrderBook Add/Cancel/Modify/Clear/Trade/Fill behavior;
- BookManager multi-instrument routing and missing-instrument order lookup;
- LobMarketDataEventProcessor snapshots and final summary;
- StandardRunner + LOB synthetic NDJSON integration;
- Flat/hierarchy LOB digest equivalence, including equal timestamp tie-breakers;
- AsyncSnapshotWriter queue draining and sync/async LOB snapshot equivalence;
- ResultPrinter standard-mode timing and throughput;
- Flat merge chronological ordering;
- Hierarchical merge chronological ordering.

Run:

```bash
./run test
```

## Feather Conversion

Use `scripts/convert_to_feather.py` to convert Databento/XEUR NDJSON files into one Feather file per input file:

```bash
python3 scripts/convert_to_feather.py \
  --input data/XEUR-20260409-HTT6HHLT6R \
  --output data_feather/XEUR-20260409-HTT6HHLT6R
```

The script reads `.mbo.json`, `.jsonl`, and `.ndjson` files recursively, preserves row order within each file, and writes the relevant L3 fields:

```text
ts_recv, ts_event, instrument_id, order_id, side, action, price, size
```

`ts_event` and `instrument_id` are also read from Databento's nested `hd` object when they are not present at the top level. `price` is stored as a string column so quoted decimal prices and integer fixed-point prices are preserved exactly; JSON `null` prices remain Arrow nulls. The script requires `pyarrow`.

Validated conversion result for `data/XEUR-20260409-HTT6HHLT6R`:

```text
converted_files=22
total_rows=19869718
total_json_lines=19869718
per_file_row_counts_match=true
first_last_ts_recv_preserved=true
first_last_action_side_preserved=true
null_price_preserved=true
```

To compare Python-side read cost for NDJSON parsing versus Feather reads:

```bash
python3 scripts/benchmark_feather_read.py \
  --json-input data/XEUR-20260409-HTT6HHLT6R \
  --feather-input data_feather/XEUR-20260409-HTT6HHLT6R
```

Validated read benchmark on the same folder:

```text
Format,Files,Rows,WallClockSeconds,RowsPerSecond
json,22,19869718,55.574674,357531.89
feather,22,19869718,1.515689,13109362.05
```

These are measured values for this machine and cache state; use the actual run output in reports rather than assuming a fixed speedup.

## Optional C++ Feather Input

Arrow C++ support is optional and kept out of the default build:

```bash
cmake -S . -B build-arrow -DCMAKE_BUILD_TYPE=Release -DENABLE_ARROW=ON
cmake --build build-arrow -j
ctest --test-dir build-arrow --output-on-failure
```

The Arrow-enabled unit test suite includes an opt-in real-folder check. To run it, set `MD_RUN_REAL_FEATHER_TEST=1`; `MD_REAL_JSON_FOLDER` and `MD_REAL_FEATHER_FOLDER` can override the default `data/...` and `data_feather/...` paths.

When `ENABLE_ARROW=ON`, hard-task modes can read Feather folders directly:

```bash
./build-arrow/ingest --mode flat \
  --input data_feather/XEUR-20260409-HTT6HHLT6R \
  --input-format feather \
  --lob \
  --verbose

./build-arrow/ingest --mode hierarchy \
  --input data_feather/XEUR-20260409-HTT6HHLT6R \
  --input-format feather \
  --lob \
  --verbose
```

The Feather producer maps each row into the same `MarketDataEvent` shape as the JSON parser: `timestamp = ts_recv` when present, otherwise `ts_event`; prices are converted to the fixed `1e-9` int64 convention; Arrow null prices become `UNDEF`; side/action strings are mapped to the project enums; and source metadata is assigned as `(source_file_id, row_index)`.

Validated real-folder JSON vs Feather LOB benchmark:

```text
Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,WallClockSeconds,ThroughputMessagesPerSecond,LobDigest
flat,json,lob,19869718,0,0,5.147601,3859995.85,0xf068b976ad32134e
hierarchy,json,lob,19869718,0,0,6.070810,3272993.05,0xf068b976ad32134e
flat,feather,lob,19869718,0,0,4.675216,4250010.52,0xf068b976ad32134e
hierarchy,feather,lob,19869718,0,0,3.624509,5482043.36,0xf068b976ad32134e
```

The matching `LobDigest` confirms that JSON and Feather producers reconstruct the same final LOB state for this dataset.

The combined C++ JSON vs Feather benchmark can be produced with:

```bash
./scripts/benchmark.sh \
  data/XEUR-20260409-HTT6HHLT6R \
  data_feather/XEUR-20260409-HTT6HHLT6R
```

## Future improvements

- Propagate exceptions from producer and merger threads back to the owning runner with clear diagnostics and a non-zero exit status.
- Add structured cancellation so one failed pipeline stage stops the rest of the ingestion pipeline quickly.
- Tune queue handoff overhead with wider fan-in or larger batches.
- Evaluate a low-latency wait policy for the hottest SPSC queue boundaries.
- Consider dispatcher parallelization only if the global ordering contract can be relaxed or partitioned safely.

## Known assumptions and limitations

- Input files are NDJSON: one JSON object per line.
- Hard-task input files are already sorted internally.
- The parser is intentionally scoped to flat JSON objects, which matches the market-data rows used by this task.
- `benchmark` mode suppresses event printing to avoid measuring stdout overhead.
