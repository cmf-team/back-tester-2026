# HW2 — Multi-instrument LOB reconstruction (hard + sharded bonus + Feather)

## Summary

Реализация Homework 2 поверх задела из **#29** (HW1: data-ingestion layer).
До мерджа #29 этот PR содержит как task1, так и task2 изменения; после мерджа
#29 я ребейзну ветку, и diff схлопнется до task2-only. Ревьюверам прошу
смотреть в первую очередь новые директории `src/order_book/`,
`src/dispatcher/`, `scripts/`, и новые тесты `test/{LimitOrderBook,Dispatcher,ShardedDispatcher}Test.cpp`.

## Что сделано

### Hard core (требования §1–§6)

- `LimitOrderBook` — L2 book на `std::map<int64_t, int64_t>` с `std::greater<>` для bids; signed-delta API (LOB stateless по orders, сохраняет инвариант "best at begin()" без отдельного отслеживания).
- `Dispatcher` — single-threaded роутер: per-instrument `unordered_map<iid, LOB>`, `unordered_map<order_id, OrderState>` для резолва Cancel/Modify/Trade/Fill без iid (требование "MarketDataEvent might not carry the instrument id").
- `SnapshotWorker` — отдельный поток для печати BBO-снапшотов (stateless I/O); сборка снапшота — O(N_instruments) в dispatcher-thread, печать — асинхронно через `SpscRingQueue<SnapshotTask>`.
- Reuse из task1: `MarketDataEvent`, `IEventSource`, `SpscRingQueue`, `FileProducer`, `IMerger`/`FlatMerger`/`HierarchyMerger`, `runHardTask` со strict-chronological invariant и FNV-fingerprint'ом.
- Точка интеграции: одна строка в `HardTask.cpp` — добавлен опциональный `EventSink` callback в `runHardTask` (default-empty → bit-for-bit task1 поведение).

### Bonus: Advanced Parallelism Option

- `ShardedDispatcher` — N worker-потоков, каждый владеет своим `unordered_map<iid, LOB>`. Routing по `hash(iid) % N`: per-instrument chronological order сохраняется без локов на горячем пути. Router-thread держит общий `order_to_iid_` cache (single writer, no lock). Каждый worker имеет свою SPSC очередь и свой `OrderState` cache.

### Additional Hard Part: Feather Conversion

- `scripts/convert_to_feather.py` — pyarrow-based транскодер NDJSON → Feather. Schema = **полный mirror** `MarketDataEvent` (15 полей, включая `sequence` для strict ordering и `symbol`). `--benchmark` flag меряет JSON-read vs Feather-read.
- `src/market_data/ArrowFeatherSource.{hpp,cpp}` — C++ producer на `arrow::ipc::RecordBatchFileReader` (требование "C++ producer reads from Feather files using the Arrow C++ library", помечено как highly recommended). Включается через `cmake -DENABLE_ARROW=ON` (по умолчанию OFF — не тащит libarrow в обычные сборки).
- `cmake/ThirdPartyLibs.cmake` — Arrow подтягивается через `ExternalProject_Add` в стиле Catch2 в этом проекте. Минимальный набор фич (IPC + LZ4/ZSTD, без Parquet/JSON/compute/dataset) → ~5 минут на первую сборку.

### CLI

```
back-tester data.mbo.json                                   # Standard task (1 file)
back-tester data/dir/ --strategy=both                       # Hard task no-LOB (Task 1 baseline)
back-tester data/dir/ --strategy=flat --lob                 # Hard + per-instrument LOBs
back-tester data/dir/ --strategy=flat --lob --snapshot-every=100000
back-tester data/dir/ --strategy=flat --sharded=4           # Bonus parallelism
back-tester data/dir/ --benchmark                           # Comparison table
back-tester data/file.feather                               # ArrowFeatherSource (ENABLE_ARROW=ON)
```

`--benchmark` запускает все четыре пайплайна (no-LOB / sequential / sharded(2) / sharded(4)) для каждой merge-стратегии и **проверяет, что все четыре fingerprint'а совпадают**. Это и есть ответ на "compare performance with the sequential map version" из задания.

## Что НЕ покрыто (явно отложено)

- C++ Arrow reader сам по себе работает (smoke-tested в memory), но full e2e ArrowFeatherSourceTest зависит от `ENABLE_ARROW=ON` и тяжёлой сборки libarrow — добавлю в follow-up если ревьюверы запросят. Python-only бенчмарк JSON vs Feather закрывает обязательную часть требования.

## Test plan

```bash
# Базовая сборка (без Arrow): ~15 секунд
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

# Полная сборка (с C++ Arrow Feather reader): ~5 минут первая, секунды далее
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DENABLE_ARROW=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

- [x] **75 unit tests green** (51 task1 + 24 новых task2)
- [x] **ASan + UBSan clean** на синтетических NDJSON
- [x] **Fingerprint match** между no-LOB / sequential / sharded(2) / sharded(4) — strict ordering invariant сохранён шардингом
- [x] `pre-commit run --all-files` — clang-format + ruff проходят
- [x] **Smoke run** на 3 файлах × 3 строки даёт ожидаемые BBO для трёх инструментов

```
=== --benchmark === (synthetic, 9 events across 3 instruments × 3 files)
fingerprint_match=yes      # все 4 пайплайна совпадают побитово
```

## Структура коммитов

1. `style: apply clang-format to existing task1 files` — bookkeeping (формат-only).
2. `feat: implement Task 2 hard variant (LOB + per-instrument dispatcher + sharded bonus)` — основная работа.
3. `feat(feather): convert_to_feather.py + optional C++ ArrowFeatherSource` — feather pipeline.

Total: ~1700 LOC новых исходников + ~800 LOC тестов.

## После мерджа #29

Я ребейзну `hw2/lob-reconstruction` на свежий upstream/main:

```bash
git fetch upstream
git rebase upstream/main hw2/lob-reconstruction
git push --force-with-lease origin hw2/lob-reconstruction
```

Diff PR'а автоматически сожмётся до task2-only (~600 строк production кода + Python скрипт + тесты).
