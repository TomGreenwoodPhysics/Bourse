#pragma once
#include "bourse/types.hpp"
#include "bourse/event.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bourse {

struct Order {
    OrderId   id;
    Side      side;
    Price     price;
    Quantity  remaining;
    Seq       seq;
    Timestamp ts;
    friend bool operator==(const Order&, const Order&) = default;
};

struct Level {
    Price       price;
    Quantity    quantity;
    std::size_t order_count;
};

struct BookSnapshot {
    std::vector<Level> bids;
    std::vector<Level> asks;
};

class OrderBook {
public:
    // record_events == true keeps an in-order log for deterministic replay.
    // Off by default so the hot path and benchmark stay allocation-light.
    explicit OrderBook(bool record_events = false);

    std::pair<OrderId, std::vector<Trade>>
    submit_limit(Side side, Price price, Quantity qty, Timestamp ts = 0);

    std::vector<Trade>
    submit_market(Side side, Quantity qty, Timestamp ts = 0);

    bool cancel(OrderId id, Timestamp ts = 0);

    std::optional<Price>  best_bid() const;
    std::optional<Price>  best_ask() const;
    std::optional<Price>  spread() const;
    std::optional<double> mid() const;

    BookSnapshot snapshot(std::size_t depth = 0) const;

    const std::vector<Trade>& trades() const noexcept { return trades_; }
    Seq         sequence() const noexcept { return seq_; }
    std::size_t resting_orders() const noexcept { return index_.size(); }

    // --- replay / determinism ---
    bool recording() const noexcept { return record_; }
    const std::vector<Event>& events() const noexcept { return events_; }

    // Resting orders in a canonical order: bids best->worst, then asks
    // best->worst, FIFO within each level. Stable basis for equality checks.
    std::vector<Order> resting_dump() const;

    // Rebuild a book by applying an event log in order.
    static OrderBook replay(const std::vector<Event>& log, bool record_events = false);

private:
    using LevelList = std::list<Order>;
    using BidMap    = std::map<Price, LevelList, std::greater<Price>>;
    using AskMap    = std::map<Price, LevelList, std::less<Price>>;

    struct Locator { Side side; Price price; LevelList::iterator it; };

    OrderId next_id_ = 1;
    Seq     seq_     = 0;
    bool    record_  = false;
    BidMap  bids_;
    AskMap  asks_;
    std::unordered_map<OrderId, Locator> index_;
    std::vector<Trade> trades_;
    std::vector<Event> events_;

    Seq next_seq() noexcept { return ++seq_; }

    template <class OppMap>
    void match(OppMap& opp, Side taker_side, OrderId taker_id,
               Price limit, bool is_market, Timestamp taker_ts,
               Quantity& remaining, std::vector<Trade>& out);

    template <class SideMap>
    void rest(SideMap& book, Side side, OrderId id, Price price,
              Quantity qty, Timestamp ts);

    template <class SideMap>
    static void collect(const SideMap& book, std::size_t depth,
                        std::vector<Level>& out);
};

} // namespace bourse