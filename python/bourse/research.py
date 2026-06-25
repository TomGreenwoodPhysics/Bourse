"""Phase 2 research layer.

Drives the matching engine with a simple randomised order flow, captures the
top of book over time into a DataFrame, and plots it. The flow here is a
placeholder for plumbing/visualisation only — the realistic synthetic market
(Poisson arrivals, volatility regimes, shocks) arrives in Phase 3.
"""

from __future__ import annotations

import random

import pandas as pd
import matplotlib.pyplot as plt

from . import OrderBook, Side


def run_random_session(
    n_steps: int = 4000,
    seed: int = 7,
    p_market: float = 0.20,
    price_lo: int = 9_900,
    price_hi: int = 10_100,
    qty_max: int = 10,
    sample_every: int = 1,
):
    """Run a randomised session and return (book, dataframe-of-top-of-book)."""
    rng = random.Random(seed)
    book = OrderBook()
    rows = []

    for step in range(n_steps):
        side = Side.Buy if rng.random() < 0.5 else Side.Sell
        if rng.random() < p_market:
            book.submit_market(side, rng.randint(1, qty_max))
        else:
            book.submit_limit(side, rng.randint(price_lo, price_hi),
                              rng.randint(1, qty_max))

        if step % sample_every == 0:
            rows.append({
                "step":     step,
                "seq":      book.sequence(),
                "best_bid": book.best_bid(),
                "best_ask": book.best_ask(),
                "mid":      book.mid(),
                "spread":   book.spread(),
                "n_trades": len(book.trades()),
                "resting":  book.resting_orders(),
            })

    return book, pd.DataFrame(rows)


def plot_top_of_book(df: pd.DataFrame, ax=None):
    """Mid-price line with the bid-ask spread shaded around it."""
    if ax is None:
        _, ax = plt.subplots(figsize=(10, 4))
    ax.fill_between(df["step"], df["best_bid"], df["best_ask"],
                    color="tab:blue", alpha=0.15, label="bid-ask spread")
    ax.plot(df["step"], df["mid"], color="tab:blue", lw=0.8, label="mid")
    ax.set_xlabel("step")
    ax.set_ylabel("price (ticks)")
    ax.set_title("Top of book over the session")
    ax.legend(loc="upper left")
    return ax


def plot_depth(book, depth: int = 12, ax=None):
    """Resting quantity at each price level: bids (green) vs asks (red)."""
    if ax is None:
        _, ax = plt.subplots(figsize=(10, 4))
    snap = book.snapshot(depth)
    if snap.bids:
        ax.bar([l.price for l in snap.bids], [l.quantity for l in snap.bids],
               width=0.9, color="tab:green", alpha=0.7, label="bids")
    if snap.asks:
        ax.bar([l.price for l in snap.asks], [l.quantity for l in snap.asks],
               width=0.9, color="tab:red", alpha=0.7, label="asks")
    ax.set_xlabel("price (ticks)")
    ax.set_ylabel("resting quantity")
    ax.set_title(f"Order book depth (top {depth} levels per side)")
    ax.legend(loc="upper right")
    return ax