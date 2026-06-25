"""Bourse — limit order book exchange simulator (Python research layer).

The C++ matching engine is exposed via the compiled `_bourse` extension.
The synthetic market, agent ladder, and evaluation tooling build on top of it.
"""

__version__ = "0.1.0"

try:
    from ._bourse import (  # noqa: F401
        OrderBook, Side, Trade, Order, Level, BookSnapshot, Event, EventKind,
    )
except ImportError as exc:  # pragma: no cover
    raise ImportError(
        "The compiled _bourse extension isn't built yet.\n"
        "From the project root run:  pip install -e . --no-deps --no-build-isolation\n"
        "(or bash scripts/build_ext.sh). See README Phase 2."
    ) from exc

__all__ = [
    "OrderBook", "Side", "Trade", "Order", "Level", "BookSnapshot",
    "Event", "EventKind",
]