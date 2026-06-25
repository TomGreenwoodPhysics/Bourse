"""End-to-end Phase 2 demo: run a session, print a summary, save charts."""

import os
import matplotlib.pyplot as plt

from bourse.research import run_random_session, plot_top_of_book, plot_depth

book, df = run_random_session(n_steps=4000, seed=11)

print(df.tail(5).to_string(index=False))
print(f"\ntrades generated: {len(book.trades())} | "
      f"resting orders: {book.resting_orders()} | "
      f"final mid: {book.mid()}")

fig, (ax_top, ax_depth) = plt.subplots(2, 1, figsize=(10, 8))
plot_top_of_book(df, ax=ax_top)
plot_depth(book, depth=12, ax=ax_depth)
fig.tight_layout()

os.makedirs("notebooks", exist_ok=True)
out = os.path.join("notebooks", "session.png")
fig.savefig(out, dpi=120)
print(f"saved {out}")