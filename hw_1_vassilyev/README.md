# HW1 — Market Data Ingestion (Standard Task)

## Objective

Basic data-ingestion layer for an event-driven backtester. Reads a single daily NDJSON L3 file, parses each message in chronological order, and creates `MarketDataEvent` objects.

## Project Structure

```
hw_1_vassilyev/
├── CMakeLists.txt          # Build config, fetches nlohmann/json
├── MarketDataEvent.hpp     # MarketDataEvent class definition
├── main.cpp                # Entry point: reads file, parses, prints
└── README.md
```

## MarketDataEvent Class

Defined in `MarketDataEvent.hpp`. Maps the relevant fields from the Databento NDJSON L3 format:

| Field           | Type       | Source JSON field       | Description                                      |
|-----------------|------------|-------------------------|--------------------------------------------------|
| `timestamp`     | `string`   | `hd.ts_event`          | Event timestamp (ISO 8601, nanosecond precision) |
| `order_id`      | `string`   | `order_id`             | Exchange order identifier                        |
| `side`          | `char`     | `side`                 | `B` = Bid, `A` = Ask, `N` = None                |
| `price`         | `double`   | `price`                | Order price (0.0 if null, e.g. for resets)       |
| `size`          | `uint32_t` | `size`                 | Order quantity                                   |
| `action`        | `char`     | `action`               | `R`=Reset, `A`=Add, `C`=Cancel, `M`=Modify, `T`=Trade, `F`=Fill |
| `instrument_id` | `uint32_t` | `hd.instrument_id`     | Numeric instrument identifier                    |
| `symbol`        | `string`   | `symbol`               | Human-readable symbol name                       |

## Program Flow (`main.cpp`)

1. **CLI argument** — expects exactly one argument: path to a single `.mbo.json` file.
2. **Read line-by-line** — opens the file and reads it as NDJSON (one JSON object per line).
3. **Parse & create events** — each line is parsed with `nlohmann::json`. The relevant fields are extracted into a `MarketDataEvent` instance. Lines that fail to parse are silently skipped.
4. **Consumer function** — `processMarketDataEvent(const MarketDataEvent&)` prints the event fields (timestamp, order_id, side, price, size, action).
5. **Output** — prints the first 10 and last 10 events, then a summary: total message count, first timestamp, last timestamp.

The first/last 10 events are tracked using two `std::deque` containers: `first10` is filled until it reaches 10 elements, `last10` is a sliding window that always keeps the most recent 10.

## Dependencies

- **C++20** (standard library only beyond the JSON parser)
- **nlohmann/json v3.11.3** — fetched automatically by CMake via `FetchContent`

## Build & Run

```bash
cd hw_1_vassilyev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/hw1_ingestion ../data/XEUR-20260409-HJTR7RCAKT/xeur-eobi-20260309.mbo.json
```

## Sample Output

```
=== First 10 events ===
ts=2026-03-09T07:52:41.367824437Z order_id=0 side=N price=0 size=0 action=R
ts=2026-03-09T07:52:41.367824437Z order_id=10996414798222631105 side=B price=0.0212 size=20 action=A
...

=== Last 10 events ===
ts=2026-03-09T18:00:02.630075425Z order_id=10996451237732085035 side=B price=0.02495 size=20 action=C
ts=2026-03-09T18:00:02.630075425Z order_id=1773079200877309227 side=A price=0.02537 size=20 action=C

=== Summary ===
Total messages processed: 1305607
First timestamp: 2026-03-09T07:52:41.367824437Z
Last timestamp:  2026-03-09T18:00:02.630075425Z
```
