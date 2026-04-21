## Summary

Adds the **data-ingestion layer** of the event-driven backtester — the first
milestone. Reads one daily zstd-compressed L3/MBO dump, decodes every
`TOrderlog` record into a `MarketDataEvent`, and feeds it through a
consumer callback in chronological order.

- `src/common/MarketDataEvent.{hpp,cpp}` — event type mirroring the 112-byte
  packed `TOrderlog` numpy dtype one-to-one (trivial layout, fixed-size
  symbol buffer; no `std::string` inside so the event is cheap to copy and
  later poolable).
- `src/common/BasicTypes.hpp` — new `Action` enum (`A/C/M/T/F/R`) with values
  equal to the feed's ASCII codes, so the decoder is a direct cast.
- `src/ingestion/L3FileReader.{hpp,cpp}` — streaming
  `ZSTD_decompressStream` reader with a small carry buffer for records
  straddling decompression-chunk boundaries. `L3RecordLayout` pins every
  byte offset via `static_assert`, so any future layout drift fails to
  compile. Fields are read via `memcpy` because the records are unaligned.
- `src/main/main.cpp` — CLI entry point taking a single path argument,
  wiring `processMarketDataEvent(const MarketDataEvent&)` as the consumer,
  and printing the first 10, last 10 events plus total count and first/last
  `ts_event` at the end.
- `cmake/ThirdPartyLibs.cmake` — pulls `zstd` v1.5.6 as a static
  `ExternalProject_Add` (same pattern as `Catch2`). No system
  `libzstd-dev` required. Links the `.a` by absolute path so a stray
  `libzstd.so` on `LD_LIBRARY_PATH` can never be picked up by accident.
- `test/MarketDataEventTest.cpp` + `test/CMakeLists.txt` — round-trip
  decode test against a hand-crafted 112-byte record, side/action mapping
  test, and symbol-view round-trip. Catch2 discovery switched to
  `PRE_TEST` so the build step never depends on the runtime loader.
- `src/ingestion/README.md` — per-module README: input format, design,
  usage, CLI, build (including the system-glibc / g++-10 path).

The code has been split into a reusable library (`back-tester-ingestion`)
so the next milestone (LOB engine) will only need to replace the
`processMarketDataEvent` stub in `main.cpp`.

## Verification against the Python reference

Compared message counts and the first event against
`scripts/convert.XEUR.ipynb`:

| File | Python `len(L)` | C++ `Messages processed` |
|------|----------------:|-------------------------:|
| `xeur-eobi-20260309.mbo.zst` | 2,232,542 | **2,232,542** |
| `xeur-eobi-20260406.mbo.zst` |       205 |       **205** |

First event of `20260309` matches `Fut.iloc[1]` in cell 16 of the notebook:

```
order_id=10996386599372464268  side=B  price=1.1528  symbol=FCEU SI 20260316 PS
```

Throughput: ~2.23M messages in 144 ms on a single core (including zstd
decompression), end-to-end.

## Test plan

- [x] `cmake -B build -S . && cmake --build build -j` succeeds with the
      Anaconda GCC 11 toolchain.
- [x] `cmake -B build -S . -DCMAKE_C_COMPILER=/usr/bin/gcc-10
      -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 && cmake --build build -j`
      also succeeds on Ubuntu 20.04 system toolchain; resulting binary
      links only against `/lib/x86_64-linux-gnu/` (no `RPATH`, no
      `anaconda3` paths — verified via `ldd` and `readelf -d`).
- [x] `build/bin/test/back-tester-tests` — **24 assertions in 4 test
      cases pass**.
- [x] `build/bin/back-tester data/opt/xeur-eobi-20260309.mbo.zst` prints
      first 10 + last 10 events + summary; counts match Python reference.
- [x] `build/bin/back-tester data/opt/xeur-eobi-20260406.mbo.zst`
      (tiny file, 205 msgs) — sanity.

## Out of scope (follow-ups)

- Limit Order Book engine — will be wired into the same consumer hook in
  a subsequent milestone.
- Multi-day / multi-file driver.
- Pooling / lock-free queue for the `MarketDataEvent` path once the LOB
  engine is in place.
