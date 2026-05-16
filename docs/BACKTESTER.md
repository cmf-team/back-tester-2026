# Backtester

## Scope

The backtester reuses the existing market data replay pipeline and adds a strategy-aware processor on top of the limit order book. Standard, flat, and hierarchy runners still own input discovery, parsing, ordering, and timing. Backtest mode swaps the event processor for `BacktestMarketDataEventProcessor`.

## Event Flow

For each `MarketDataEvent`:

1. Apply the event to `BookManager`.
2. Build a compact `MarketView` with best bid, best ask, mid, spread, microprice, and top-of-book sizes.
3. Check fills for existing simulated orders.
4. Apply fills to `Portfolio` and `BacktestMetricsCollector`.
5. Call the selected `Strategy`.
6. Apply strategy actions as simulated limit order placements or cancellations.
7. Observe market state for final PnL and inventory metrics.

This preserves the LOB replay behavior and keeps strategy logic away from internal book storage.

## Execution Model

The first execution simulator follows the assignment assumption:

- Buy limit order fills when `best_ask <= buy_limit_price`.
- Sell limit order fills when `best_bid >= sell_limit_price`.
- Partial fills are disabled.
- Fill quantity is the full remaining quantity.
- Fill price is the order limit price.

Future versions can add opposite-best fill prices, queue position, partial fills, latency, fees, and rebates.

## Metrics

Backtest reports include:

- `events_processed`
- `orders_placed`
- `orders_cancelled`
- `fills`
- `final_inventory`
- `turnover`
- `cash`
- `mark_to_market_pnl`
- `max_inventory`
- `average_inventory`
- `wall_clock_seconds`
- `throughput_messages_per_second`

Inventory path metrics use absolute total inventory sampled after each processed market event.

## CLI

Example:

```bash
./build/ingest --mode standard \
  --input tests/test_data/lob_standard_synthetic.ndjson \
  --backtest \
  --strategy avellaneda_stoikov \
  --instrument-id 1 \
  --order-size 1 \
  --tick-size 1000000000 \
  --quote-interval-events 1 \
  --max-inventory 10 \
  --gamma 0.1 \
  --sigma 1.0 \
  --k 1.0 \
  --horizon-seconds 1.0
```

Supported strategies:

- `fixed_quote`
- `avellaneda_stoikov`
- `microprice_avellaneda_stoikov`

Sample JSON configs are provided in `configs/`. They are reproducible parameter records and include the exact CLI command to run.

## Sample Data

The small deterministic sample dataset for smoke experiments is:

```text
tests/test_data/lob_standard_synthetic.ndjson
```

The larger Databento-style JSON datasets under `data/XEUR-*` can be used with the same CLI shape once the target `instrument_id` and tick size are selected.
