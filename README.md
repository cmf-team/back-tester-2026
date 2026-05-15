# Market Data Ingestion

A C++20 data-ingestion layer for an event-driven backtester. The project supports both the Standard task and the Hard task in one codebase:

- **Standard mode** reads one Databento NDJSON L3 file, creates `MarketDataEvent` objects, calls `processMarketDataEvent`, and prints the required summary plus the first/last 10 events.
- **Flat mode** reads a folder of chronologically sorted daily files with one producer thread per file and performs a single-level k-way merge.
- **Hierarchy mode** uses the same producers but merges streams through a binary tree of smaller mergers.
- **Benchmark mode** runs both Hard-task strategies and reports message count, wall-clock time, and throughput.

The project has no third-party runtime dependency. The parser is a small flat JSON-object parser tuned for NDJSON market-data rows, so reviewers do not need to install `nlohmann/json` or any package beyond a C++20 compiler and CMake.

## Quick start

```bash
./run standard data/sample.ndjson
./run flat data/multi
./run hierarchy data/multi
./run benchmark data/multi
./run test
./scripts/benchmark.sh
```

The `run` script builds the project first and then executes the requested mode. `scripts/benchmark.sh` builds a release binary and benchmarks the bundled `data/daily_20` folder by default.

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
```

Optional flags:

```bash
--verbose          Print parser diagnostics to stderr/stdout diagnostics section.
--print-events N   Print the first N events observed by processMarketDataEvent.
```

`benchmark` mode suppresses per-event logging by default because printing every event would dominate the measured runtime.

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
Strategy,Messages,ChronologicalViolations,WallClockSeconds,ThroughputMessagesPerSecond
standard,1305607,0,0.256,5100027.34
flat,15175617,0,0.934796,16234154.21
hierarchy,15175617,0,0.955811,20598115.56
```

## Benchmark results

Benchmark environment:
- OS: macOS 15.6 (Darwin 24.6.0 arm64)
- CPU: Apple M4 Max, 16 logical CPUs (12 performance + 4 efficiency cores)
- Build: Release, -O3
- Input: 22 local Databento .mbo.json NDJSON files (4.9 GB) for Hard-task modes; Standard measured on a single 1.3 GB merged NDJSON file
- Event printing: disabled
- Data location: local APFS filesystem

Latest benchmark output:

| Strategy | Messages | Chronological violations | Wall-clock seconds | Throughput messages/s |
| --- | ---: | ---: | ---: | ---: |
| standard | 1,305,607 | 0 | 0.256000 | 5,100,027.34 |
| flat | 15,175,617 | 0 | 0.934796 | 16,234,154.21 |
| hierarchy | 15,175,617 | 0 | 0.955811 | 20,598,115.56 |

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

The current implementation is `LoggingMarketDataEventProcessor`. In the future, it can be replaced with a LOB updater, a statistics collector, or a mid-price calculator without changing the ingestion and merge code.

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
- Flat merge chronological ordering;
- Hierarchical merge chronological ordering.

Run:

```bash
./run test
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
