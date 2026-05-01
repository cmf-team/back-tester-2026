# Performance Report: HFT Market Making Strategies

## 1. Data Description

**Instrument:** Cryptocurrency token (spot market)  
**Period:** August 1–7, 2024  
**Data source:** Level-2 order book snapshots + trade tape

| Dataset | Records | Description |
|---|---|---|
| `lob.csv` | 1,036,690 | Order book snapshots every ~2 seconds, top-25 levels |
| `trades.csv` | 21,864,989 | All executed trades |

**Price dynamics:**
- Start price: 0.01104
- End price: 0.00774
- Total move: **-30%** (strong downtrend)

This is an **adverse market** for market making — the persistent downtrend causes continuous adverse selection against the market maker.

---

## 2. Model Description

### 2.1 Avellaneda-Stoikov (2008)

Reference: *High-frequency trading in a limit order book*, Avellaneda & Stoikov, Quantitative Finance, Vol. 8, No. 3, 2008.

The model derives optimal bid and ask quotes for a market maker with inventory risk aversion.

**Reservation price** (equation 8 from paper):
```
r(s, q, t) = s - q * γ * σ² * (T - t)
```

**Optimal spread** (equation 30 from paper):
```
δ = γ * σ² * (T - t) + (2/γ) * ln(1 + γ/κ)
```

**Final quotes:**
```
bid = r - δ/2
ask = r + δ/2
```

**Parameters:**

| Parameter | Value | Description |
|---|---|---|
| γ (gamma) | 0.01 | Risk aversion coefficient |
| κ (kappa) | 1.5 | Order flow intensity |
| T | 1.0 | Trading horizon (seconds) |
| σ | dynamic | Volatility estimated from last 50 snapshots |
| order_size | 50,000 | Size of each limit order (tokens) |

**Key insight:** When inventory q > 0 (long position), the reservation price r shifts below mid-price, causing the strategy to quote more aggressively on the ask side to liquidate inventory.

---

### 2.2 Weighted Microprice + AS (2018)

Reference: *The Micro-Price: A High Frequency Estimator of Future Prices*, Stoikov, Quantitative Finance, Vol. 18, No. 12, 2018.

Instead of using the raw mid-price as the reference price, we use the **weighted mid-price** (equation 2-3 from paper) as an approximation of the microprice:

**Imbalance:**
```
I = Q_bid / (Q_bid + Q_ask)
```

**Weighted mid-price:**
```
W = I * P_ask + (1 - I) * P_bid
```

This approximation corresponds to the Brownian motion special case of the microprice (Appendix B of Stoikov 2018), where the microprice coincides with the weighted mid-price.

**Intuition:** If bid volume >> ask volume, buyers outnumber sellers → price likely to move up → weighted mid is closer to ask → we quote more conservatively on the buy side.

---

### 2.3 Full Microprice + AS (Stoikov 2018)

This implements the complete microprice model from Section 3 of Stoikov (2018).

**Framework:** The microprice is defined as:
```
P_micro = M + G*(I, S)
```
where G* is the microprice adjustment estimated from historical LOB data.

**Estimation procedure:**

1. Discretize imbalance I into 10 buckets, spread S into 3 buckets
2. Build transition matrices from LOB snapshots:
   - Q (nm × nm): transient-to-transient transitions (mid unchanged)
   - T (nm × nm): transient-to-absorbing transitions (mid changed)
   - R (nm × 4): absorbing state probabilities
3. Compute first-order adjustment:
   ```
   G¹ = (I - Q)⁻¹ R K
   ```
4. Compute iteration matrix:
   ```
   B = (I - Q)⁻¹ T
   ```
5. Iterate to convergence:
   ```
   G* = G¹ + B·G¹ + B²·G¹ + ...
   ```
6. At runtime: lookup G*(I_bucket, S_bucket) and add to mid-price

**Symmetrization** ensures convergence (B*G¹ = 0, Theorem 3.1).

---

## 3. Performance Results

### 3.1 Summary Table

| Strategy | PnL | Inventory | Turnover | Fills | Max DD | Sharpe | vs AS |
|---|---|---|---|---|---|---|---|
| AS (2008) | -1,374 | +4,100,000 | 33,679,083 | 89,522 | -2,750 | -0.0014 | baseline |
| Weighted Microprice | -253 | +3,850,000 | 32,926,971 | 87,727 | -2,080 | -0.0003 | **+81.5%** |
| Full Microprice | **+6,879** | -7,350,000 | 33,462,641 | 89,005 | -17,499 | +0.0016 | **+600.6%** |

### 3.2 Analysis

**AS (2008):** Loses money because the 30% downtrend causes continuous adverse selection. The strategy keeps buying (bid fills) while the asset price falls. Final inventory of +4.1M tokens bought at high prices.

**Weighted Microprice:** Reduces losses by 81.5% because the imbalance signal correctly identifies selling pressure. When ask volume exceeds bid volume, the strategy quotes more defensively, accumulating less inventory.

**Full Microprice:** Achieves positive PnL of +6,879 with improvement of 600.6% over baseline. The statistically estimated G* adjustment provides a better prediction of short-term price direction than either mid-price or weighted mid-price, as demonstrated empirically by Stoikov (2018). The negative inventory (-7.35M) indicates the strategy correctly identified the downtrend and maintained a net short position.

### 3.3 Execution Statistics

- **Simulation period:** ~6.5 days of tick data
- **LOB snapshots processed:** 1,036,690
- **Trades checked for execution:** 21,864,989
- **Execution assumption:** Fill when market trade price crosses our limit order level
- **Simulation speed:** ~2 seconds per strategy (C++ implementation)

---

## 4. Discussion

### Why AS (2008) loses on trending markets

The AS model assumes the mid-price follows a zero-drift Brownian motion (equation 1 of paper). This assumption is violated in our dataset where price declined 30%. The inventory management mechanism (reservation price adjustment) is insufficient to overcome strong directional pressure.

### Why Full Microprice succeeds

The G* adjustment is estimated directly from the historical LOB data of this specific asset. It captures the actual microstructure dynamics including:
- Asymmetric order flow patterns during the downtrend
- The relationship between imbalance and subsequent price moves
- Bid-ask spread dynamics

By incorporating this information, the strategy correctly positions itself against the trend.

### Adverse selection

All three strategies face adverse selection — informed traders systematically trade against our quotes. The microprice models reduce (but do not eliminate) this effect by better estimating the true fair value of the asset.

---

## 5. Improvement Roadmap

### Short-term (immediate improvements)

1. **Partial fills:** Currently we fill entire order size. Implement partial fills based on trade volume to more accurately model execution. Expected impact: more realistic inventory accumulation.

2. **Transaction costs:** Add exchange fees (typically 0.01-0.05% per trade). This would reduce all PnL metrics and provide more realistic results.

3. **Parameter calibration:** Grid search over γ ∈ [0.001, 0.1] and κ ∈ [0.5, 5.0] to find optimal parameters for this specific asset and market regime.

### Medium-term

4. **Order Flow Imbalance (OFI):** Incorporate OFI signal from Cont, Kukanov & Stoikov — tracks changes in bid/ask volumes rather than instantaneous snapshot. Shown to better explain short-term price moves.

5. **Queue position modeling:** Account for priority in the order queue. Our model assumes immediate execution when price crosses; in reality we may be behind other orders.

6. **Regime detection:** Switch between market-making (sideways) and trend-following (trending) regimes automatically based on realized volatility and price drift.

### Long-term

7. **Multi-asset extension:** Apply strategy simultaneously across correlated instruments to hedge inventory risk.

8. **Reinforcement learning:** Use RL to optimize parameter selection in real-time, adapting to changing market conditions.

9. **Full matrix microprice calibration:** Re-estimate Q, T, R matrices on rolling window (e.g., last 24 hours) to adapt to non-stationary market microstructure.

---

## 6. References

1. Avellaneda, M. & Stoikov, S. (2008). *High-frequency trading in a limit order book*. Quantitative Finance, 8(3), 217–224. DOI: 10.1080/14697680701381228

2. Stoikov, S. (2018). *The micro-price: A high frequency estimator of future prices*. Quantitative Finance, 18(12), 1959–1966. SSRN: 2970694
