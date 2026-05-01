#!/usr/bin/env python3
"""
Compute full Stoikov (2018) microprice adjustment table G*(I, S)
from LOB snapshots.
"""

import numpy as np
import pandas as pd
import sys

# ── Parameters ──────────────────────────────────────────────────────────────
N_IMBALANCE = 10
N_SPREAD = 3
MAX_ITER = 100
TOL = 1e-8


def discretize_imbalance(qb, qa):
    total = qb + qa
    if total <= 0:
        return N_IMBALANCE // 2

    imbalance = qb / total
    bucket = int(imbalance * N_IMBALANCE)
    return min(bucket, N_IMBALANCE - 1)


def discretize_spread(spread, tick_size):
    if tick_size <= 0:
        return 0

    ticks = round(spread / tick_size)

    if ticks <= 1:
        return 0
    if ticks == 2:
        return 1

    return min(2, N_SPREAD - 1)


def state_index(i_bucket, s_bucket):
    return i_bucket * N_SPREAD + s_bucket


def main():
    lob_path = sys.argv[1] if len(sys.argv) > 1 else "data/lob.csv"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "data/microprice_table.csv"

    print(f"Reading {lob_path}...")
    df = pd.read_csv(lob_path)
    print(f"  Loaded {len(df)} snapshots")

    bid_col = "bids[0].price"
    ask_col = "asks[0].price"
    bvol_col = "bids[0].amount"
    avol_col = "asks[0].amount"

    bids = df[bid_col].values
    asks = df[ask_col].values
    bvols = df[bvol_col].values
    avols = df[avol_col].values

    mids = (bids + asks) / 2.0
    spreads = asks - bids

    tick_size = float(np.median(spreads[spreads > 0]))
    print(f"  Estimated tick size: {tick_size:.8f}")

    nm = N_IMBALANCE * N_SPREAD

    K_values = np.array([-tick_size, -tick_size / 2, tick_size / 2, tick_size])

    Q_counts = np.zeros((nm, nm))
    T_counts = np.zeros((nm, nm))
    R_counts = np.zeros((nm, len(K_values)))

    print("  Building transition matrices...")
    n = len(df)

    for i in range(n - 1):
        x_i = discretize_imbalance(bvols[i], avols[i])
        x_s = discretize_spread(spreads[i], tick_size)
        x = state_index(x_i, x_s)

        y_i = discretize_imbalance(bvols[i + 1], avols[i + 1])
        y_s = discretize_spread(spreads[i + 1], tick_size)
        y = state_index(y_i, y_s)

        d_mid = mids[i + 1] - mids[i]

        if abs(d_mid) < tick_size * 0.1:
            Q_counts[x, y] += 1
        else:
            T_counts[x, y] += 1
            k_idx = np.argmin(np.abs(K_values - d_mid))
            R_counts[x, k_idx] += 1

    print("  Symmetrizing...")
    for i in range(N_IMBALANCE):
        for s in range(N_SPREAD):
            x = state_index(i, s)
            x_sym = state_index(N_IMBALANCE - 1 - i, s)

            Q_counts[x_sym] += Q_counts[x][::-1]
            R_counts[x_sym] += R_counts[x][::-1]

    row_sums_Q = Q_counts.sum(axis=1, keepdims=True)
    row_sums_T = T_counts.sum(axis=1, keepdims=True)

    total = row_sums_Q + row_sums_T
    total = np.where(total == 0, 1, total)

    Q = Q_counts / total
    T = T_counts / total
    R = R_counts / total

    print("  Computing G1...")
    IQ = np.eye(nm) - Q

    try:
        IQ_inv = np.linalg.inv(IQ)
    except np.linalg.LinAlgError:
        IQ_inv = np.linalg.pinv(IQ)

    G1 = IQ_inv @ R @ K_values

    B = IQ_inv @ T

    print("  Computing G*...")
    G_star = G1.copy()
    Bk_G1 = G1.copy()

    for it in range(MAX_ITER):
        Bk_G1 = B @ Bk_G1
        prev = G_star.copy()
        G_star += Bk_G1

        if np.max(np.abs(G_star - prev)) < TOL:
            print(f"  Converged at iteration {it + 1}")
            break

    rows = []
    for i in range(N_IMBALANCE):
        for s in range(N_SPREAD):
            x = state_index(i, s)
            imbalance_center = (i + 0.5) / N_IMBALANCE

            rows.append(
                {
                    "imbalance_bucket": i,
                    "spread_bucket": s,
                    "imbalance_center": imbalance_center,
                    "G_star": G_star[x],
                }
            )

    out_df = pd.DataFrame(rows)
    out_df.to_csv(out_path, index=False)

    print(f"\nSaved microprice table to {out_path}")
    print(
        f"\nG* summary:\n"
        f"  min={G_star.min():.8f}  max={G_star.max():.8f}  mean={G_star.mean():.8f}"
    )

    print("\nSample (imbalance vs G*):")
    for i in range(N_IMBALANCE):
        x = state_index(i, 0)
        print(
            f"  I_bucket={i}  "
            f"I_center={((i + 0.5) / N_IMBALANCE):.2f}  "
            f"G*={G_star[x]:.8f}"
        )


if __name__ == "__main__":
    main()
