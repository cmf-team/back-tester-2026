# Backtest Results

## Experiment Setup

Dataset:

```text
tests/test_data/lob_standard_synthetic.ndjson
```

Instrument:

```text
1
```

The sample dataset is intentionally tiny and deterministic. It is useful for validating the engine and report shape; it is not a profitability benchmark. On this dataset the market does not cross the simulated quotes before requoting, so all three strategies produce zero fills and zero PnL.

## Commands

Fixed quote baseline:

```bash
./build/ingest --mode standard --input tests/test_data/lob_standard_synthetic.ndjson --backtest --strategy fixed_quote --instrument-id 1 --order-size 1 --tick-size 1000000000 --quote-offset-ticks 1 --quote-interval-events 1 --max-inventory 10
```

Avellaneda-Stoikov 2008:

```bash
./build/ingest --mode standard --input tests/test_data/lob_standard_synthetic.ndjson --backtest --strategy avellaneda_stoikov --instrument-id 1 --order-size 1 --tick-size 1000000000 --quote-interval-events 1 --max-inventory 10 --gamma 0.1 --sigma 1.0 --k 1.0 --horizon-seconds 1.0
```

Microprice Avellaneda-Stoikov:

```bash
./build/ingest --mode standard --input tests/test_data/lob_standard_synthetic.ndjson --backtest --strategy microprice_avellaneda_stoikov --instrument-id 1 --order-size 1 --tick-size 1000000000 --quote-interval-events 1 --max-inventory 10 --gamma 0.1 --sigma 1.0 --k 1.0 --horizon-seconds 1.0 --imbalance-skew --imbalance-alpha-ticks 0.5
```

## Comparison

```csv
Strategy,Events,Fills,Orders,Cancels,FinalInventory,Turnover,MTM_PnL,Seconds,Throughput
fixed_quote,8,0,12,10,0,0.000000,0.000000,0.000703,11372.375647
avellaneda_stoikov,8,0,12,10,0,0.000000,0.000000,0.000031,254898.837024
microprice_avellaneda_stoikov,8,0,12,10,0,0.000000,0.000000,0.000037,214793.932071
```

Wall-clock timing is environment-dependent and should be interpreted only as a smoke-scale run result.

## Next Experiment

Run the same three configs on a full Databento JSON session under `data/XEUR-*` with a real target instrument id and tick size. That run should replace the smoke table above for the final project report.
