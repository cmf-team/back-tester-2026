# orderbook

`orderbook` is a small Cython/C++ extension for building an L3 limit order book
from an order-log stream. It is optimized for fast replay of Databento-like
market-by-order records and can emit custom results through callbacks on every
processed update.

The package keeps two books in memory:

- `Bid`: bid-side book, sorted by price;
- `Ask`: ask-side book, sorted by price.

Each price level stores individual orders as `order_id -> size`, so the module
can handle add, cancel, modify, and reset messages at L3 granularity.

## Repository Layout

```text
orderbook/
├── orderbook.pyx          # Cython implementation and Python API
├── orderbook.pxd          # Cython declarations for C++ maps and callbacks
├── setup.py               # setuptools extension build
├── pyproject.toml         # build-system requirements
├── requirements.txt       # runtime/dev dependencies
└── examples/              # notebooks with usage examples
```

## Requirements

- Python 3.9+
- C++ compiler with C++ support
- `numpy`
- `cython`
- `wheel` / `setuptools`

The current build flags are tuned for Linux/x86_64 and use native CPU
optimizations such as `-march=native`, `-mavx2`, and `-mfma`.

## Installation

From the `orderbook` directory:

```bash
python -m pip install -r requirements.txt
python -m pip install .
```

For local development, install the extension in editable mode:

```bash
python -m pip install -e .
```

If the generated extension becomes stale after editing `orderbook.pyx` or
`orderbook.pxd`, rebuild it by reinstalling the package.

## Input Format

The parser expects a NumPy structured array compatible with `TOrderlog`.

```python
import orderbook

orderbook.TOrderlog
```

Fields:

| Field | Type | Description |
| --- | --- | --- |
| `ts_event` | `datetime64[ns]` | Exchange/event timestamp |
| `ts_recv` | `datetime64[ns]` | Receive timestamp |
| `action` | `S1` | Message action |
| `side` | `S1` | Book side: `b"B"` or `b"A"` |
| `price` | `float64` | Order price |
| `size` | `int64` | Order size or cancel size |
| `channel_id` | `int32` | Source channel id |
| `order_id` | `uint64` | Unique order id |
| `flags` | `uint8` | Feed flags |
| `ts_in_delta` | `int32` | Timestamp delta |
| `sequence` | `int32` | Sequence number |
| `symbol` | `S45` | Symbol |
| `rtype` | `int32` | Record type |
| `publisher_id` | `uint32` | Publisher id |
| `instrument_id` | `int32` | Instrument id |

Supported actions:

- `b"A"`: add order when `price > 0` and `size > 0`;
- `b"C"`: cancel/reduce order by `size`;
- `b"M"`: modify order price and size;
- `b"R"`: reset both bid and ask books.

## Basic Usage

Return best bid/ask quotes for every valid book state:

```python
import numpy as np
import orderbook

records = np.array([...], dtype=orderbook.TOrderlog)

quotes = orderbook.parseL3(records, orderbook.aBidAsk())

for bid, ask in quotes:
    print(bid, ask)
```

Use the generator API when you do not want to materialize all callback results:

```python
for bid, ask in orderbook.gparseL3(records, orderbook.aBidAsk()):
    print(bid, ask)
```

Build snapshots of the top price levels:

```python
snapshots = orderbook.parseL3(
    records,
    orderbook.aSnapL3(),
    height=10,
)

for bid_levels, ask_levels in snapshots:
    print(bid_levels, ask_levels)
```

## Python API

### `parseL3(Orderlog, callback_addr, **kwargs) -> list`

Replays all records, updates bid/ask books, calls the callback after each
record, and returns a list of non-`None` callback results.

### `gparseL3(Orderlog, callback_addr, **kwargs) -> Generator`

Generator version of `parseL3`. It yields each non-`None` callback result as
soon as it is produced.

### `aBidAsk() -> int`

Returns the address of the built-in callback that emits `(bid, ask)` when both
sides are present and `ask > bid`.

### `aSnapL3() -> int`

Returns the address of the built-in callback that emits `(Bid, Ask)` snapshots
up to `height` levels. This is useful for inspection and debugging, but it is
slower than scalar callbacks because it converts book levels to Python objects.

## Custom Callbacks

Callbacks are Cython functions with this signature:

```cython
cdef object callback(
    int index,
    COrderlog &rec,
    CBookL3 &Bid,
    CBookL3 &Ask,
    dict kwargs,
)
```

The callback is called after each record is applied. Return `None` to skip
emitting a result for the current row, or return any Python object to append it
to `parseL3` results / yield it from `gparseL3`.

The callback address is passed to Python as an integer, following the pattern
used by the built-in helpers:

```cython
def aMyCallback() -> np.int64:
    return <intptr_t>myCallback
```

## Notes

- Prices are stored as `double`; avoid feeding mixed tick precision if exact
  price-level equality is important.
- Cancel messages reduce the order by the provided `size`; fully cancelled
  orders and empty price levels are removed.
- Modify messages replace the current order price/size. If the old price is not
  found, the module searches the book by `order_id`.
- Reset messages clear both books.

## Examples

See notebooks in `orderbook/examples/` for exploratory usage:

- `get.orderbook.ipynb`
- `npTick.ipynb`
- `npSnap.ipynb`
