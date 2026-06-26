"""Synthetic market simulator (Phase 3).

Drives the C++ matching engine with order flow that *looks* like a market,
replacing the uniform-random flow of the Phase 2 demo. The design is a
**latent-fair-value** model:

  * A hidden fair value ``V`` follows a mean-reverting process whose volatility
    and drift depend on the current market **regime** (calm / volatile /
    trending). Occasional **liquidity shocks** jump it.
  * **Market makers** (background liquidity) quote a ladder of limit orders on
    both sides around ``V``, widening their spread in volatile regimes. They
    fully refresh each step, so the visible book stays anchored to ``V`` and
    never self-trades on a jump.
  * **Noise takers** send Poisson-arrival market orders in random directions.
  * **Informed takers**, active mostly in volatile/trending regimes, trade in
    the direction of ``V - mid`` — buying when the market is cheap relative to
    fair value. This is the adverse-selection pressure Phase 4's market-making
    agents will have to survive.

``V`` is the ground truth: Phase 4 can score signal quality and adverse
selection against it.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import IntEnum

import numpy as np
import pandas as pd

from . import OrderBook, Side


class Regime(IntEnum):
    CALM = 0
    VOLATILE = 1
    TREND_UP = 2
    TREND_DOWN = 3


@dataclass(frozen=True)
class RegimeParams:
    name: str
    sigma: float          # fair-value volatility per step (ticks)
    drift: float          # fair-value drift per step (ticks)
    noise_rate: float     # mean number of noise market orders per step (Poisson)
    informed_rate: float  # mean number of informed orders per step when a burst is active
    informed_prob: float  # per-step probability an informed burst is active
    spread_mult: float    # maker half-spread multiplier


DEFAULT_REGIMES = {
    Regime.CALM:       RegimeParams("calm",       0.6,  0.0,  5.0, 0.0, 0.00, 1.0),
    Regime.VOLATILE:   RegimeParams("volatile",   2.6,  0.0,  9.0, 3.0, 0.20, 2.2),
    Regime.TREND_UP:   RegimeParams("trend_up",   1.1,  0.7,  7.0, 3.0, 0.30, 1.4),
    Regime.TREND_DOWN: RegimeParams("trend_down", 1.1, -0.7,  7.0, 3.0, 0.30, 1.4),
}


@dataclass
class MarketConfig:
    anchor: float = 10_000.0          # long-run fair value (ticks)
    kappa: float = 0.01               # mean-reversion speed of fair value
    base_half_spread: int = 2         # baseline maker half-spread (ticks)
    maker_levels: int = 8             # price levels quoted per side
    maker_size_max: int = 8           # max size per maker order
    taker_size_max: int = 4           # max size per taker order
    regime_switch_prob: float = 0.004 # per-step probability of a regime change
    shock_prob: float = 0.0015        # per-step probability of a liquidity shock
    shock_scale: float = 30.0         # stdev of a shock jump (ticks)
    seed: int | None = 7
    record_events: bool = False       # keep the engine's replay log


class SyntheticMarket:
    """A regime-switching, latent-value synthetic market over the engine."""

    def __init__(self, config: MarketConfig | None = None,
                 regimes: dict[Regime, RegimeParams] | None = None):
        self.cfg = config or MarketConfig()
        self.regimes = regimes or DEFAULT_REGIMES
        self.rng = np.random.default_rng(self.cfg.seed)
        self.book = OrderBook(self.cfg.record_events)

        self.fair = float(self.cfg.anchor)
        self.regime = Regime.CALM
        self.ts = 0
        self._step = 0
        self._bid_ids: deque[int] = deque()
        self._ask_ids: deque[int] = deque()
        self._rows: list[dict] = []

    # --- dynamics -----------------------------------------------------------
    def _advance_fair(self, p: RegimeParams) -> None:
        pull = self.cfg.kappa * (self.cfg.anchor - self.fair)
        shock = 0.0
        if self.rng.random() < self.cfg.shock_prob:
            shock = float(self.rng.normal(0.0, self.cfg.shock_scale))
        self.fair += pull + p.drift + p.sigma * float(self.rng.normal()) + shock

    def _maybe_switch_regime(self) -> None:
        if self.rng.random() < self.cfg.regime_switch_prob:
            others = [int(r) for r in self.regimes if r != self.regime]
            self.regime = Regime(int(self.rng.choice(others)))

    def _refresh_quotes(self, p: RegimeParams) -> None:
        # Full refresh: pull all our resting quotes, then repost around fair.
        # Cancelling everything first means a jump in `fair` can never make a
        # new order cross one of our own stale ones (no self-trades).
        for oid in self._bid_ids:
            self.book.cancel(oid, self.ts)
        for oid in self._ask_ids:
            self.book.cancel(oid, self.ts)
        self._bid_ids.clear()
        self._ask_ids.clear()

        center = int(round(self.fair))
        half = max(1, int(round(self.cfg.base_half_spread * p.spread_mult)))
        for k in range(self.cfg.maker_levels):
            bsize = int(self.rng.integers(1, self.cfg.maker_size_max + 1))
            asize = int(self.rng.integers(1, self.cfg.maker_size_max + 1))
            bid_id, _ = self.book.submit_limit(Side.Buy,  center - half - k, bsize, self.ts)
            ask_id, _ = self.book.submit_limit(Side.Sell, center + half + k, asize, self.ts)
            self._bid_ids.append(bid_id)
            self._ask_ids.append(ask_id)

    def _takers(self, p: RegimeParams) -> None:
        size_max = self.cfg.taker_size_max
        # Noise: undirected Poisson market-order flow.
        for _ in range(int(self.rng.poisson(p.noise_rate))):
            side = Side.Buy if self.rng.random() < 0.5 else Side.Sell
            self.book.submit_market(side, int(self.rng.integers(1, size_max + 1)), self.ts)
        # Informed: directed toward fair value when a burst is active.
        if self.rng.random() < p.informed_prob:
            mid = self.book.mid()
            if mid is not None and self.fair != mid:
                side = Side.Buy if self.fair > mid else Side.Sell
                for _ in range(int(self.rng.poisson(p.informed_rate))):
                    self.book.submit_market(side, int(self.rng.integers(1, size_max + 1)), self.ts)

    # --- recording ----------------------------------------------------------
    def _record(self) -> None:
        bid, ask = self.book.best_bid(), self.book.best_ask()
        snap = self.book.snapshot(1)
        bq = snap.bids[0].quantity if snap.bids else 0
        aq = snap.asks[0].quantity if snap.asks else 0
        micro = imb = None
        if bid is not None and ask is not None and (bq + aq) > 0:
            micro = (bid * aq + ask * bq) / (aq + bq)   # size-weighted fair price
            imb = (bq - aq) / (bq + aq)
        self._rows.append({
            "step": self._step,
            "ts": self.ts,
            "regime": int(self.regime),
            "regime_name": self.regimes[self.regime].name,
            "fair_value": self.fair,
            "best_bid": bid,
            "best_ask": ask,
            "mid": self.book.mid(),
            "microprice": micro,
            "spread": (ask - bid) if (bid is not None and ask is not None) else None,
            "top_imbalance": imb,
            "n_trades": len(self.book.trades()),
            "resting": self.book.resting_orders(),
        })

    # --- driver -------------------------------------------------------------
    def step(self) -> None:
        p = self.regimes[self.regime]
        self.ts += 1
        self._advance_fair(p)
        self._maybe_switch_regime()
        self._refresh_quotes(p)
        self._takers(p)
        self._record()
        self._step += 1

    def run(self, n_steps: int = 5000):
        for _ in range(n_steps):
            self.step()
        return self.book, pd.DataFrame(self._rows)


# --- plotting ---------------------------------------------------------------
_REGIME_TINT = {0: "#e8f5e9", 1: "#ffebee", 2: "#e3f2fd", 3: "#fff3e0"}


def _shade_regimes(ax, df) -> None:
    reg = df["regime"].to_numpy()
    steps = df["step"].to_numpy()
    start = 0
    for i in range(1, len(reg) + 1):
        if i == len(reg) or reg[i] != reg[start]:
            ax.axvspan(steps[start], steps[i - 1] + 1,
                       color=_REGIME_TINT.get(int(reg[start]), "#ffffff"), alpha=0.6, lw=0)
            start = i


def plot_session(df, book=None, path: str | None = None):
    """Two panels: fair value vs mid (regime-shaded) and the spread over time."""
    import matplotlib.pyplot as plt

    fig, (ax_p, ax_s) = plt.subplots(2, 1, figsize=(11, 7), height_ratios=[3, 1], sharex=True)

    _shade_regimes(ax_p, df)
    ax_p.plot(df["step"], df["fair_value"], color="black", lw=1.0, label="fair value V")
    ax_p.plot(df["step"], df["mid"], color="tab:blue", lw=0.7, alpha=0.9, label="mid")
    ax_p.set_ylabel("price (ticks)")
    ax_p.set_title("Synthetic market: latent fair value vs traded mid (shaded by regime)")
    ax_p.legend(loc="upper left")

    _shade_regimes(ax_s, df)
    ax_s.plot(df["step"], df["spread"], color="tab:purple", lw=0.7)
    ax_s.set_ylabel("spread")
    ax_s.set_xlabel("step")

    fig.tight_layout()
    if path:
        fig.savefig(path, dpi=120)
    return fig