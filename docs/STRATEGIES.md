# Strategies

## Fixed Quote Baseline

`FixedQuoteStrategy` is a deterministic engine check:

- If the market has a mid price, cancel old quotes.
- Place a buy at `mid - quote_offset_ticks * tick_size`.
- Place a sell at `mid + quote_offset_ticks * tick_size`.
- Stop placing buy quotes at or above `max_inventory`.
- Stop placing sell quotes at or below `-max_inventory`.

This strategy is intentionally simple. Its purpose is to verify order lifecycle, fill accounting, and report generation before comparing market making models.

## Avellaneda-Stoikov 2008

`AvellanedaStoikovStrategy` implements inventory-aware market making using the mid price as fair value.

Reservation price:

```text
reservation_price = mid - inventory * gamma * sigma^2 * time_remaining
```

Optimal spread:

```text
optimal_spread =
  gamma * sigma^2 * time_remaining
  + (2 / gamma) * log(1 + gamma / k)
```

Quotes:

```text
raw_bid = reservation_price - optimal_spread / 2
raw_ask = reservation_price + optimal_spread / 2
bid = round_down_to_tick(raw_bid)
ask = round_up_to_tick(raw_ask)
```

Long inventory lowers the reservation price, which makes ask quoting more aggressive and buy quoting less aggressive. Short inventory raises the reservation price and has the opposite effect.

## Microprice Avellaneda-Stoikov

`MicropriceAvellanedaStoikovStrategy` keeps the same inventory and spread terms, but replaces mid with top-of-book microprice:

```text
microprice =
  (best_ask * bid_size_at_best + best_bid * ask_size_at_best)
  / (bid_size_at_best + ask_size_at_best)
```

When bid size dominates, microprice moves toward ask. When ask size dominates, microprice moves toward bid.

The strategy also supports an optional imbalance skew:

```text
imbalance =
  (bid_size_at_best - ask_size_at_best)
  / (bid_size_at_best + ask_size_at_best)

fair_price = microprice + imbalance_alpha_ticks * imbalance * tick_size
```

Set `--imbalance-skew --imbalance-alpha-ticks X` to enable it from the CLI.

## Improvement Roadmap

- Calibrate `gamma`, `sigma`, and `k` from historical data instead of static configs.
- Estimate volatility over rolling windows per instrument.
- Estimate order arrival intensity from marketable events near touch.
- Add queue position and partial fills.
- Add maker/taker fees and rebates.
- Add latency and cancel/replace throttling.
- Add config-file loading so JSON configs can be executed directly.
- Add richer experiment output, including drawdown, fill ratio, quote lifetime, and inventory histogram.
