# backtester-2026 — Data Ingestion Layer

C++20 L3 market data ingestion layer for the Eurex EUR/USD options & futures backtester.

## Repository layout

```
include/
  MarketDataEvent.hpp   # Core event struct (price, size, action, side, timestamps)
  NdjsonParser.hpp      # Zero-dependency Databento NDJSON line parser
src/
  standard_task.cpp     # Single-file reader → processMarketDataEvent()
  hard_task.cpp         # Multi-file parallel merger (FlatMerger + HierarchyMerger)
CMakeLists.txt
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## Standard task usage

```bash
./build/standard_task data/2025-04-01_EURUSD_options_12345.json
```

Prints the first and last 10 `MarketDataEvent` objects and a processing summary.

## Hard task usage

```bash
# Run both strategies and benchmark
./build/hard_task data/daily/

# Run only one strategy
./build/hard_task data/daily/ flat
./build/hard_task data/daily/ hierarchy
```

## Design notes

### MarketDataEvent
Follows the Databento MBO schema. Prices are stored as `int64_t` in fixed-point
(1 unit = 1 × 10⁻⁹) matching the wire format; `price_decimal()` converts lazily.
The sort key is `ts_recv` (Databento's monotonic hardware timestamp) with
`ts_event` as a tiebreaker, consistent with Databento's index timestamp definition.

### Parser
`NdjsonParser` uses `std::from_chars` (no locale, no allocation) for all numeric
fields. It handles both quoted and unquoted integer fields as Databento sometimes
quotes `int64` values for JSON precision.

### FlatMerger
Classic k-way merge with a `std::priority_queue` min-heap. O(N log k) time,
O(k) memory. Each file has one producer thread pushing into a `BlockingQueue<8192>`.
The dispatcher thread pops from the heap.

### HierarchyMerger
Binary tournament tree of `MergePair` nodes. Each node runs its own background
thread that merges two `BlockingQueue` streams into one. The tree has depth
⌈log₂ k⌉. This increases parallelism but adds O(k) threads and latency from
the intermediate queues.

For k ≈ 20 files the FlatMerger is generally faster because the heap is tiny
and cache-resident. The HierarchyMerger becomes advantageous when k is large
(hundreds of files) and producers are IO-bound.
