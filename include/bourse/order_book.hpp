#pragma once
//
// Bourse — limit order book with price-time (FIFO) priority.
//
// Data structure:
//   * Each side is a std::map keyed by price, ordered so that begin() is the
//     BEST price (bids: descending, asks: ascending). O(log L) level lookup.
//   * Each price level holds a std::list<Order> in arrival order (FIFO time
//     priority). std::list gives O(1) erase and, crucially, ITERATOR STABILITY
//     so a cancel index can hold direct iterators into the level.
//   * index_ maps OrderId -> {side, price, list iterator} for O(1) cancel.
//
// This is the clear, correct reference implementation. Phase 1's job is
// correctness; the layout is deliberately chosen so a later intrusive /
// arena-allocated rewrite can be benchmarked against it without changing the
// public contract.
//
#include "bourse/types.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bourse {

// A resting order living inside a price level's FIFO queue.
struct Order {
    OrderId  id;
    Side     side;
    Price    price;
    Quantity remaining;
    Seq      seq;        // arrival sequence (tie-break within a price level)
};

// One aggregated price level, for snapshots / market-data views.
struct Level {
    Price       price;
    Quantity    quantity;     // sum of resting quantity at this price
    std::size_t order_count;  // number of resting orders at this price
};

// A depth-of-book view. bids run best->worst (descending price),
// asks run best->worst (ascending price).
struct BookSnapshot {
    std::vector<Level> bids;
    std::vector<Level> asks;
};

class OrderBook {
public:
    OrderBook() = default;

    // Submit a limit order. If marketable it crosses the opposite side
    // immediately (price-time priority); any residual rests at `price`.
    // Returns the engine-assigned order id and the trades generated.
    std::pair<OrderId, std::vector<Trade>>
    submit_limit(Side side, Price price, Quantity qty);

    // Submit a market order. Sweeps the opposite side until filled or the book
    // runs dry. Never rests; an unfilled remainder is simply dropped.
    // Returns the trades generated.
    std::vector<Trade>
    submit_market(Side side, Quantity qty);

    // Cancel a resting order. Returns true iff it existed and was removed.
    bool cancel(OrderId id);

    // --- Top-of-book accessors (nullopt when that side is empty) ---
    std::optional<Price>  best_bid() const;
    std::optional<Price>  best_ask() const;
    std::optional<Price>  spread() const;   // best_ask - best_bid
    std::optional<double> mid() const;      // (best_bid + best_ask) / 2

    // Aggregated depth. depth == 0 means "all levels".
    BookSnapshot snapshot(std::size_t depth = 0) const;

    // Full trade tape for the session (in execution order).
    const std::vector<Trade>& trades() const noexcept { return trades_; }

    // Current global event sequence (also the count of sequenced events).
    Seq sequence() const noexcept { return seq_; }

    // Live resting-order count (excludes fully-filled / cancelled orders).
    std::size_t resting_orders() const noexcept { return index_.size(); }

private:
    using LevelList = std::list<Order>;
    using BidMap    = std::map<Price, LevelList, std::greater<Price>>;
    using AskMap    = std::map<Price, LevelList, std::less<Price>>;

    struct Locator {
        Side                 side;
        Price                price;
        LevelList::iterator  it;
    };

    OrderId next_id_ = 1;
    Seq     seq_     = 0;
    BidMap  bids_;
    AskMap  asks_;
    std::unordered_map<OrderId, Locator> index_;
    std::vector<Trade> trades_;

    Seq next_seq() noexcept { return ++seq_; }

    // Match an aggressor against `opp`. Templated on the opposite side's map
    // type so one body serves both directions. `remaining` is decremented in
    // place; trades are appended to `out` (and to the session tape).
    template <class OppMap>
    void match(OppMap& opp, Side taker_side, OrderId taker_id,
               Price limit, bool is_market,
               Quantity& remaining, std::vector<Trade>& out);

    // Rest a residual order on its own side's book and index it for cancel.
    template <class SideMap>
    void rest(SideMap& book, Side side, OrderId id, Price price, Quantity qty);

    // Aggregate one side's map into snapshot levels (best -> worst).
    template <class SideMap>
    static void collect(const SideMap& book, std::size_t depth,
                        std::vector<Level>& out);
};

} // namespace bourse