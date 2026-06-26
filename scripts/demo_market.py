"""Phase 3 demo: run the synthetic market, summarise it, save a chart."""

import os

from bourse.market import SyntheticMarket, MarketConfig, plot_session

m = SyntheticMarket(MarketConfig(seed=11))
book, df = m.run(5000)

print("regime step counts:")
print(df["regime_name"].value_counts().to_string())

dev = (df["mid"] - df["fair_value"]).abs()
print(f"\ntrades: {len(book.trades())} | "
      f"mid~fair corr: {df['mid'].corr(df['fair_value']):.4f} | "
      f"mean |mid-fair|: {dev.mean():.2f} ticks | "
      f"resting now: {df['resting'].iloc[-1]}")
print("mean spread by regime:")
print(df.groupby("regime_name")["spread"].mean().to_string())

os.makedirs("notebooks", exist_ok=True)
out = os.path.join("notebooks", "market.png")
plot_session(df, book, path=out)
print(f"\nsaved {out}")