# data_ingestion

Reads Databento MBO (market-by-order) NDJSON files and produces `MarketDataEvent` objects.

---

## Components

### `MarketDataEvent` — core data struct

```cpp
struct MarketDataEvent {
    NanoTime   ts_received;      // wall-clock time packet was received (ns since epoch)
    NanoTime   ts_event;         // exchange timestamp of the event   (ns since epoch)
    Price      price;            // order price (double, 0.0 when JSON price is null)
    Quantity   qty;              // order quantity
    OrderId    order_id;         // uint64 — quoted string in JSON ("1775088023898785831")
    SecurityId instrument_id;   // from hd.instrument_id
    MarketId   market_id;        // from hd.publisher_id
    uint16_t   channel_id;
    uint32_t   sequence;
    uint8_t    flags;            // bitmask — see flags section below
    Action     action;           // Add / Modify / Cancel / Clear / Trade / Fill / None
    Side       side;             // Buy / Sell / None
    string     symbol;
};
```

### `enums.hpp` — typed enumerations

| Enum | Values | Source field |
|------|--------|-------------|
| `action::Action` | Add `A`, Modify `M`, Cancel `C`, Clear `R`, Trade `T`, Fill `F`, None `N` | `"action"` |
| `side::Side` | Buy `B`, Sell `A`, None `N` | `"side"` |
| `flags::Flag` | bitmask — see below | `"flags"` |

### Flags bitmask

| Flag | Bit | Decimal | Meaning |
|------|-----|---------|---------|
| `F_LAST`              | 7 | 128 | Last record in event for this instrument |
| `F_TOB`               | 6 |  64 | Top-of-book message, not individual order |
| `F_SNAPSHOT`          | 5 |  32 | Sourced from replay / snapshot server |
| `F_MBP`               | 4 |  16 | Aggregated price level, not individual order |
| `F_BAD_TS_RECV`       | 3 |   8 | `ts_recv` inaccurate (clock issue / packet reorder) |
| `F_MAYBE_BAD_BOOK`    | 2 |   4 | Unrecoverable channel gap — book state unknown |
| `F_PUBLISHER_SPECIFIC`| 1 |   2 | Meaning depends on `publisher_id` |

Multiple flags can be set simultaneously in one number (bitmask):

```cpp
// flags = 160 → F_LAST (128) + F_SNAPSHOT (32) are both set
bool is_last = cmf::flags::has(ev.flags, cmf::flags::F_LAST);
bool skip    = cmf::flags::should_skip(ev.flags);  // true if F_BAD_TS_RECV or F_MAYBE_BAD_BOOK
```

### `JSONParser` — line parser

Parses one NDJSON line into a `MarketDataEvent`. All parsing is pointer-based (zero heap allocation except `symbol`). No external JSON libraries used.

**Expected JSON format (Databento MBO):**
```json
{
  "ts_recv": "2026-04-02T00:00:23.898795059Z",
  "hd": {
    "ts_event":     "2026-04-02T00:00:23.898773987Z",
    "publisher_id": 101,
    "instrument_id": 436
  },
  "action":     "A",
  "side":       "B",
  "price":      "1.162900000",
  "size":       20,
  "channel_id": 23,
  "order_id":   "1775088023898785831",
  "flags":      128,
  "sequence":   124519,
  "symbol":     "FCEU SI 20260615 PS"
}
```

Notes:
- `price` may be `null` → stored as `0.0`
- `price` and `order_id` are quoted strings in Databento output
- `ts_event` and `instrument_id` are nested inside `"hd"`
- Timestamps are parsed to nanoseconds since Unix epoch without any heap allocation

### `SimpleLoader` — file reader

Reads an NDJSON file line-by-line and calls a consumer for each parsed event.

```cpp
cmf::SimpleLoader loader("xeur-eobi-20260402.mbo.json");

loader.load([](const cmf::MarketDataEvent& ev) {
    if (cmf::flags::should_skip(ev.flags)) return;
    // process ev
});
```

Malformed lines are silently skipped.

---

## Build

```bash
cmake -S . -B build
cmake --build build --target simple-loader
```

## Run

```bash
./build/bin/simple-loader <ndjson-file>
```

Prints the first and last 10 events and a summary:

```
--- Summary ---
Total messages : 577366
First ts       : 2026-04-02T06:38:37.527868463Z
Last  ts       : 2026-04-02T16:20:01.994301309Z
Elapsed (s)    : 3.10
Msg/sec        : 186143
```
