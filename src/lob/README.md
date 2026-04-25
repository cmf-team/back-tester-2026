# lob — Limit Order Book

Reconstructs a per-symbol, multi-instrument limit order book from Databento MBO NDJSON events.

Based on the O(1) design described in
["How to Build a Fast Limit Order Book" (WK Selph, 2011)](https://web.archive.org/web/20110219163448/http://howtohft.wordpress.com/2011/02/15/how-to-build-a-fast-limit-order-book/).

---

## Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Add — existing price level | O(1) | level lookup in hash map |
| Add — new price level | O(log M) | BST insert, M = distinct active levels |
| Cancel | O(1) | order lookup + doubly-linked list unlink |
| Execute / Fill | O(1) | same as cancel |
| Best bid / ask | O(1) | direct pointer |
| Volume at price | O(1) | hash map lookup |

---

## Data Structures

### `OrderEntry` — one resting order

```cpp
struct OrderEntry {
    OrderId    order_id;
    side::Side side;
    int64_t    quantity;    // resting quantity
    int64_t    price;       // scaled: raw_price * 1e9 (integer)
    NanoTime   entry_time;
    NanoTime   last_update;
    OrderEntry* next;       // newer order at this price level (towards tail)
    OrderEntry* prev;       // older order at this price level (towards head)
    PriceLevel* level;      // owning price level
};
```

All orders at the same price are kept in arrival order (FIFO) via a doubly-linked list. `prev`/`next` allow O(1) removal of any order from the middle.

### `PriceLevel` — all orders at one price

```cpp
struct PriceLevel {
    int64_t price;
    int64_t order_count;
    int64_t total_quantity;

    PriceLevel* parent, *left_child, *right_child;  // AVL BST
    int         height;

    OrderEntry* front;   // head of FIFO queue (executes first)
    OrderEntry* back;    // tail of FIFO queue (appended here)
};
```

Price levels are nodes in an AVL self-balancing BST. Bid levels and ask levels live in separate trees.

### `LimitOrderBook` — complete book for one symbol

```cpp
class LimitOrderBook {
    PriceLevel* bid_tree;   // AVL BST root, buy side
    PriceLevel* ask_tree;   // AVL BST root, sell side
    PriceLevel* best_bid;   // direct pointer — O(1)
    PriceLevel* best_ask;   // direct pointer — O(1)

    unordered_map<OrderId,  OrderEntry*> orders_;  // O(1) cancel by id
    unordered_map<int64_t,  PriceLevel*> levels_;  // O(1) add to existing level
};
```

**Price scaling**: all prices are stored as `int64_t = round(price_double * 1e9)` to avoid floating-point comparisons as hash map keys.

---

## Event Handling (`apply`)

| Action | Handler | Complexity |
|--------|---------|-----------|
| `Add`    | `add_order`    | O(1) / O(log M) for new level |
| `Cancel` | `cancel_order` | O(1) |
| `Modify` | `modify_order` | O(1) if price unchanged; O(log M) if price changed |
| `Clear`  | `clear`        | O(N) — frees all orders and levels |
| `Trade`, `Fill`, `None` | ignored | — |

Events where `flags::should_skip` is true are ignored.

---

## Multi-Instrument Routing

```cpp
std::unordered_map<std::string, LimitOrderBook> books;
books[ev.symbol].apply(ev);
```

Each symbol gets its own independent `LimitOrderBook`.

---

## Usage

```cpp
#include "lob/LimitOrderBook.hpp"
#include "data_ingestion/SimpleLoader.hpp"

std::unordered_map<std::string, cmf::LimitOrderBook> books;

cmf::SimpleLoader loader("xeur-eobi-20260402.mbo.json");
loader.load([&](const cmf::MarketDataEvent& ev) {
    if (cmf::flags::should_skip(ev.flags)) return;
    books[ev.symbol].apply(ev);
});

// Query best prices
auto& book = books["FCEU SI 20260615 PS"];
double bid = cmf::LimitOrderBook::unscale_price(book.best_bid_price());
double ask = cmf::LimitOrderBook::unscale_price(book.best_ask_price());

// Print top 5 levels
book.print_snapshot(5);
```

---

## Build

```bash
cmake -S . -B build
cmake --build build --target lob-main
```

## Run

```bash
./build/bin/lob-main <ndjson-file> [snapshot_interval=100000]
```

Prints a snapshot every `snapshot_interval` events (top 3 busiest symbols, 5 levels each),
then a final best bid/ask summary and throughput stats:

```
=== Summary ===
Total events : 577366
Symbols      : 140
First ts     : 2026-04-02T06:38:37.527868463Z
Last  ts     : 2026-04-02T16:20:01.994301309Z
Elapsed (s)  : 3.087
Events/sec   : 187057
```
