# Data ingestion layer

This module is the first stage of the event-driven backtester. It reads one
daily zstd-compressed L3/MBO file, decodes every message into a
`MarketDataEvent`, and hands it off — **in chronological order** — to a
user-supplied consumer. The Limit Order Book (LOB) engine will plug into that
same consumer slot in a later milestone.

## Contents

| File                          | Purpose                                                       |
|-------------------------------|---------------------------------------------------------------|
| `L3FileReader.hpp/.cpp`       | Streaming zstd reader + packed-record decoder.                |
| `../common/MarketDataEvent.hpp/.cpp` | Event type used across the backtester.                 |
| `../common/BasicTypes.hpp`    | `Side`, `Action`, `OrderId`, `NanoTime`, ... primitives.      |
| `../main/main.cpp`            | CLI entry point that drives one file through the pipeline.    |
| `../../test/MarketDataEventTest.cpp` | Unit tests for the event type and the record decoder.  |

## Input format

Each daily file is a zstd-compressed raw dump of a numpy structured array
produced by `scripts/convert.XEUR.ipynb`:

```python
TOrderlog = np.dtype([
    ('ts_event',     'M8[ns]'),   # int64 ns since epoch
    ('ts_recv',      'M8[ns]'),   # int64 ns since epoch
    ('action',       'S1'),       # A / C / M / T / F / R
    ('side',         'S1'),       # B / A / N
    ('price',        'f8'),
    ('size',         'i8'),
    ('channel_id',   'i4'),
    ('order_id',     'u8'),
    ('flags',        'u1'),
    ('ts_in_delta',  'i4'),
    ('sequence',     'i4'),
    ('symbol',       'S45'),
    ('rtype',        'i4'),
    ('publisher_id', 'u4'),
    ('instrument_id','i4'),
])
# TOrderlog.itemsize == 112
```

Key points:
- Records are **packed** (no alignment padding) — 112 bytes each.
- Fields are therefore **unaligned** in memory; the decoder reads every
  field via `std::memcpy` to avoid undefined behavior.
- Exact byte offsets are hard-coded in `L3RecordLayout` and
  `static_assert`ed, so layout drift would fail to compile.

## Design

- **Streaming** — we never materialize the whole decompressed file. The
  reader feeds zstd fixed-size input chunks (`ZSTD_DStreamInSize()`), pushes
  the resulting bytes into a small `carry` buffer when a record straddles a
  chunk boundary, and flushes completed records to the consumer.
- **Zero-copy field decode** — each record is decoded in place out of the
  decompression output buffer; the consumer only sees `const MarketDataEvent&`.
- **Trivial event type** — `MarketDataEvent` has no `std::string`, only a
  fixed `std::array<char, 45>` for the symbol, so the event can be cheaply
  copied, stored in ring buffers, or later moved onto a thread-safe queue.

## Usage

```cpp
#include "ingestion/L3FileReader.hpp"

void processMarketDataEvent(const cmf::MarketDataEvent& order) {
    // ... feed the LOB / strategy / logger ...
}

std::size_t n = cmf::readL3ZstFile(
    "data/opt/xeur-eobi-20260309.mbo.zst",
    processMarketDataEvent);
```

The callback signature is exactly the one required by the milestone:

```cpp
void processMarketDataEvent(const MarketDataEvent& order);
```

## CLI

```bash
build/bin/back-tester <path-to-daily-zst-file>
```

Example:

```bash
build/bin/back-tester data/opt/xeur-eobi-20260309.mbo.zst
```

Output layout:
- `=== First 10 events ===` — the first ten `MarketDataEvent`s produced.
- `=== Last 10 events ===`  — the last ten (indices continue from the
  absolute message index, so you can see the total in context).
- `=== Summary ===`         — file path, total message count, first and
  last `ts_event` (nanoseconds since epoch).

Sample:

```
=== First 10 events ===
[0] MDE{ts=2026-03-09 00:00:00.000000000 order_id=0                    side=N price=nan        size=0  action=R symbol=FCEU SI 20260316 PS}
[1] MDE{ts=2026-03-09 00:00:00.000000000 order_id=10996386599372464268 side=B price=1.15280000 size=20 action=A symbol=FCEU SI 20260316 PS}
[2] MDE{ts=2026-03-09 00:00:00.000000000 order_id=1773014562391096283  side=A price=1.15320000 size=20 action=A symbol=FCEU SI 20260316 PS}
...

=== Summary ===
File              : data/opt/xeur-eobi-20260309.mbo.zst
Messages processed: 2232542
First ts_event ns : 1773014400000000000
Last  ts_event ns : 1773089996181814509
```

The 2,232,542 count matches the numpy conversion log in
`scripts/convert.XEUR.ipynb` cell 14 (`[2232542, 1328747, ...]`), and the
first `(order_id, price, symbol)` tuple matches `Fut.iloc[1]` in cell 16 —
i.e. the C++ and Python pipelines agree on the exact same byte stream.

## Build

The project pulls `zstd` (v1.5.6, static-only) via `ExternalProject_Add` in
`cmake/ThirdPartyLibs.cmake`, so no system `libzstd-dev` is required —
a fresh clone + `cmake` is enough.

```bash
cmake -B build -S .
cmake --build build -j
```

## Tests

```bash
ctest --test-dir build -j
# or directly:
build/bin/test/back-tester-tests
```

Three cases cover the module:
1. `MarketDataEvent - symbol roundtrip` — `setSymbol` / `symbolView`.
2. `L3FileReader - decodeRecord parses packed layout` — a hand-rolled
   112-byte record is decoded and every field is verified.
3. `L3FileReader - side / action mapping` — `B/A/N` → `Side`,
   `A/C/M/T/F/R` → `Action`.

## Extending

- **Plugging the LOB engine:** replace the `processMarketDataEvent` stub in
  `src/main/main.cpp` with a call into the book update path. The reader
  itself does not need to change.
- **Multiple files / multiple days:** `readL3ZstFile` is cheap to call
  repeatedly; feeding a merged callback is the simplest way to stitch days.
- **New fields:** add them to `MarketDataEvent`, extend `L3RecordLayout`
  and `decodeRecord`, bump the `static_assert` on the record size.
