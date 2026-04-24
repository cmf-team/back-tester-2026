# High-Performance Multi-Instrument LOB Backtester

Event-driven backtester for Eurex EUR/USD options & futures using Databento L3 (MBO) data.

## What it does

- Reads ~20 daily Databento NDJSON files **in parallel** (one producer thread per file)
- Merges all events into one strict chronological stream using **two strategies**:
  - **FlatMerger** — single min-heap k-way merge, O(log k) per event
  - **HierarchyMerger** — binary tournament tree, better cache locality for large k
- Single **dispatcher thread** routes each event to the correct `LimitOrderBook`
- **Async LOB snapshots** run in detached threads (stateless — never blocks dispatcher)
- **Feather conversion**: Python script converts JSON → Apache Arrow Feather (~10–20× faster reads); C++ calls it automatically via `system()` and reports timing
- Full **benchmark output**: events processed, wall time, throughput (ev/s)

## Project structure

```
backtester/
├── src/
│   ├── common/
│   │   ├── MarketDataEvent.hpp   # event struct + Databento flags
│   │   ├── Order.hpp             # individual order
│   │   ├── EventParser.hpp/cpp   # simdjson NDJSON parser
│   │   ├── LimitOrderBook.hpp/cpp# L2 aggregated order book
│   │   ├── ThreadSafeQueue.hpp   # blocking producer/consumer queue
│   │   ├── FlatMerger.hpp        # single min-heap merger
│   │   ├── HierarchyMerger.hpp   # binary tournament tree merger
│   │   └── Dispatcher.hpp        # LOB router + async snapshots
│   └── main/
│       └── main.cpp              # full pipeline entry point
├── tests/
│   └── test_main.cpp             # Catch2 unit tests
├── scripts/
│   └── convert_to_feather.py     # JSON → Feather converter + benchmark
├── CMakeLists.txt
├── README.md
└── .gitignore
```

## Dependencies

| Library | Purpose | How it's fetched |
|---|---|---|
| [simdjson](https://github.com/simdjson/simdjson) | Fast NDJSON parsing (~2 GB/s) | CMake FetchContent |
| [Catch2 v2](https://github.com/catchorg/Catch2) | Unit tests | CMake FetchContent |
| [pyarrow](https://pypi.org/project/pyarrow/) | Feather conversion (Python) | `pip install pyarrow` |

> **Why simdjson?** Hand-rolled string parsing works but a third-party library is the right tool here — simdjson is purpose-built for NDJSON, parses lazily (zero-copy on-demand API), and is ~5× faster than nlohmann/json.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

For debug + sanitizers:
```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)
```

## Run

```bash
# Main pipeline (adjust path to your data directory)
./build/bin/backtester data/extracted/

# The program will:
# [1] Read all JSON files in parallel
# [2] Benchmark FlatMerger vs HierarchyMerger
# [3] Run full pipeline, print first/last 10 events + stats
# [4] Convert JSON → Feather and report read-speed comparison
```

## Tests

```bash
cmake --build build --target tests
ctest --test-dir build --output-on-failure
# or run directly:
./build/bin/tests
```

Test coverage:
- `LimitOrderBook`: Add, Cancel, Modify, Trade, Fill, Reset, partial cancel, level aggregation
- `FlatMerger`: single/two/three streams, empty stream, tie timestamps
- `HierarchyMerger`: single/two/four streams
- Merger agreement: FlatMerger and HierarchyMerger produce identical output
- `ThreadSafeQueue`: FIFO ordering, done signal, producer/consumer threads
- `MarketDataEvent`: price decimal conversion, flags, type strings

## Expected output

```
=== High-Performance Multi-Instrument LOB Backtester ===
Data directory : data/extracted/
Files found    : 20

[1] Reading 20 file(s) in parallel...
  [producer 0] xeur-eobi-20260301.mbo.json → 1423847 events
  ...
  Total parsed: 28476940 events in 4.123 s

[2] Merger benchmark (merge-only, no LOB):
  FlatMerger            28476940 events   2.341 s    12163500 ev/s
  HierarchyMerger       28476940 events   2.187 s    13017800 ev/s

[3] Full pipeline  (FlatMerger → Queue → Dispatcher → LOB):
    First 10 and last 10 events:
  [     1] ts=1740787200000000000  iid=123456    oid=98765432     Add    Bid   price=0.0216      qty=100
  ...

[3] Pipeline summary:
    Instruments (LOBs) : 23
    Events processed   : 28476940
    Wall time          : 5.847 s
    Throughput         : 4870200 ev/s

=== FINAL BEST BID/ASK PER INSTRUMENT ===
  instrument 123456        bid=0.021630         ask=0.021640

[4] Feather conversion & read-speed comparison
  Running: python3 scripts/convert_to_feather.py --benchmark data/extracted/
  xeur-eobi-20260301.mbo.json: 1423847 records | JSON 312.4 MB → Feather 89.1 MB (29%) | 8.21s
  --- Read benchmark ---
  JSON    :   1423847 records   8.213s     173,357 rec/s
  Feather :   1423847 records   0.412s   3,456,900 rec/s  ↑ 19.9x faster
```

## Key design decisions

**Strict chronological order** is guaranteed by using `ts_recv` (Databento's monotonic receive timestamp) as the sort key, not `ts_event` (exchange timestamp, which can be non-monotonic across gateways).

**instrument_id routing**: Cancel/Trade/Fill events may arrive with `instrument_id=0` if the matching Add was in an earlier message. The dispatcher maintains an `order_id → instrument_id` cache to resolve this.

**LOB updates are always sequential** — only the dispatcher thread touches LOB state, so no locking is needed on the books themselves. Snapshots copy the LOB value and run in detached threads.
