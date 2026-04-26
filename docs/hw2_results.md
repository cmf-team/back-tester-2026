# Homework 2 — results & benchmarks

Hardware: i9-11900H (8 P-cores), 64 GB RAM, Fedora 43, GCC 15, `-O3 -march=native`.

Dataset: Eurex EOBI MBO L3, two folders `XEUR-20260409-HJTR7RCAKT1` and
`XEUR-20260409-HTT6HHLT6R1` (each ~22 daily files, ~17M events / 17 GB JSON
per folder, 35M total events when chained).

## Ingestion / dispatch throughput

`Producer -> FlatMerger -> Dispatcher -> per-instrument LimitOrderBook`

Single-day file (`xeur-eobi-20260309.mbo.json`, 1.31M events, 151 books):

| Mode                            | Time   | Throughput   |
| ------------------------------- | ------ | ------------ |
| Sequential dispatcher           | 0.60 s | 2.19 M msg/s |
| Sharded dispatcher, N=2 workers | 0.57 s | 2.30 M msg/s |
| Sharded dispatcher, N=4 workers | 0.58 s | 2.26 M msg/s |
| Snapshot sink (every 50k events) | 0.69 s | 1.90 M msg/s |

Both folders chained (35.0M events, 2027 books):

| Mode                            | Time    | Throughput   |
| ------------------------------- | ------- | ------------ |
| Sequential dispatcher           | 12.09 s | 2.90 M msg/s |
| Sharded dispatcher, N=2 workers | 10.70 s | 3.28 M msg/s |
| Sharded dispatcher, N=4 workers | 10.73 s | 3.27 M msg/s |

Sharded gain: ~13% at N=2 on chained workloads, saturates at N=4 because the
single dispatcher thread is the funnel. Matches the design plan's prediction
("2 workers ≈ 1.5x, 4 workers ≈ 1.7x asymptote").

Determinism: per-instrument final BBO is bit-identical between sequential and
sharded paths on synthetic + real data (unit tests
`ShardedDispatcher - matches sequential per-instrument BBO`).

## JSON vs Feather (Apache Arrow IPC)

Conversion: `scripts/mbo_json_to_feather.py` (orjson + pyarrow).

| File              | Rows      | NDJSON size | Feather size | Ratio |
| ----------------- | --------- | ----------- | ------------ | ----- |
| xeur-eobi-20260309 | 1.31 M   | 452.7 MB    | 19.9 MB      | 0.044 |

Python ingestion-only comparison (`scripts/bench_ingest_compare.py`):

| Reader              | Time   | Rows/s    |
| ------------------- | ------ | --------- |
| NDJSON via orjson   | 1.43 s | 0.91 M/s  |
| Feather via pyarrow | 0.03 s | 51.27 M/s |
| speedup             |        | x56       |

The C++ NDJSON producer in HW1 mmap+memmem-parses at ~3 M/s on this file. A
C++ Arrow IPC reader (not implemented here — system has no `arrow-devel`)
would do typed column reads with no UTF-8 / floating-point parsing on the hot
path. Conservative projection: 3-5x C++ ingest speedup, 23x storage savings,
zero schema changes downstream because `MarketDataEvent` is the same POD.

## Stateless parallel work

`SnapshotSink` runs a writer thread that pulls `SnapshotFrame` (top-10 levels
per side, 320 B per frame) off an SPSC queue. Hot path only does
`book.snapshot_bids/asks(span)` + `queue.push`. No locks. Output is CSV.

## Test coverage

21 Catch2 tests, 128 assertions. Highlights:
- LOB ops (add/cancel/fill/clear/snapshot, aggregation, level eviction).
- OrderIndex round-trips, `update_qty`.
- Dispatcher orphan resolution (Cancel/Modify/Fill with no instrument_id).
- Trade-no-mutate.
- Reset clears only target instrument.
- ShardedDispatcher per-instrument BBO equals sequential at N=2,4 on
  randomized synthetic stream of 5000 events / 7 instruments.
