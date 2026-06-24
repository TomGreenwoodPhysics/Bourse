//
// Bourse — OrderBook implementation.
//
#include "bourse/order_book.hpp"

#include <algorithm>
#include <iterator>

namespace bourse {

// --------------------------------------------------------------------------
// Matching core
// --------------------------------------------------------------------------
template <class OppMap>
void OrderBook::match(OppMap& opp, Side taker_side, OrderId taker_id,
                      Price limit, bool is_market,
                      Quantity& remaining, std::vector<Trade>& out) {
    while (remaining > 0 && !opp.empty()) {
        auto  level_it    = opp.begin();          // best price on the opposite side
        const Price level_price = level_it->first;

        // Marketable check. Market orders ignore the limit and take any price.
        const bool marketable =
            is_market ||
            (taker_side == Side::Buy ? level_price <= limit
                                     : level_price >= limit);
        if (!marketable) break;

        LevelList& fifo = level_it->second;        // arrival-ordered queue
        while (remaining > 0 && !fifo.empty()) {
            Order&   maker = fifo.front();         // oldest order = time priority
            const Quantity fill = std::min(remaining, maker.remaining);

            Trade t{maker.id, taker_id, level_price, fill, taker_side, next_seq()};
            out.push_back(t);
            trades_.push_back(t);

            remaining       -= fill;
            maker.remaining -= fill;

            if (maker.remaining == 0) {            // maker fully consumed
                index_.erase(maker.id);
                fifo.pop_front();
            }
            // (else: taker exhausted; maker keeps its queue position)
        }

        if (fifo.empty()) opp.erase(level_it);     // drop emptied price level
    }
}

// --------------------------------------------------------------------------
// Resting & indexing
// --------------------------------------------------------------------------
template <class SideMap>
void OrderBook::rest(SideMap& book, Side side, OrderId id,
                     Price price, Quantity qty) {
    LevelList& fifo = book[price];                 // creates level if absent
    fifo.push_back(Order{id, side, price, qty, next_seq()});
    auto it = std::prev(fifo.end());
    index_.emplace(id, Locator{side, price, it});
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------
std::pair<OrderId, std::vector<Trade>>
OrderBook::submit_limit(Side side, Price price, Quantity qty) {
    const OrderId id = next_id_++;
    std::vector<Trade> out;
    if (qty <= 0) return {id, out};

    Quantity remaining = qty;
    if (side == Side::Buy) match(asks_, Side::Buy,  id, price, false, remaining, out);
    else                   match(bids_, Side::Sell, id, price, false, remaining, out);

    if (remaining > 0) {
        if (side == Side::Buy) rest(bids_, side, id, price, remaining);
        else                   rest(asks_, side, id, price, remaining);
    }
    return {id, out};
}

std::vector<Trade>
OrderBook::submit_market(Side side, Quantity qty) {
    const OrderId id = next_id_++;
    std::vector<Trade> out;
    if (qty <= 0) return out;

    Quantity remaining = qty;
    if (side == Side::Buy) match(asks_, Side::Buy,  id, 0, true, remaining, out);
    else                   match(bids_, Side::Sell, id, 0, true, remaining, out);
    // Market residual is dropped (no resting).
    return out;
}

bool OrderBook::cancel(OrderId id) {
    auto idx = index_.find(id);
    if (idx == index_.end()) return false;

    const Locator loc = idx->second;
    if (loc.side == Side::Buy) {
        auto lvl = bids_.find(loc.price);
        if (lvl != bids_.end()) {
            lvl->second.erase(loc.it);
            if (lvl->second.empty()) bids_.erase(lvl);
        }
    } else {
        auto lvl = asks_.find(loc.price);
        if (lvl != asks_.end()) {
            lvl->second.erase(loc.it);
            if (lvl->second.empty()) asks_.erase(lvl);
        }
    }
    index_.erase(idx);
    return true;
}

// --------------------------------------------------------------------------
// Top of book
// --------------------------------------------------------------------------
std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const {
    auto b = best_bid(), a = best_ask();
    if (!b || !a) return std::nullopt;
    return *a - *b;
}

std::optional<double> OrderBook::mid() const {
    auto b = best_bid(), a = best_ask();
    if (!b || !a) return std::nullopt;
    return (static_cast<double>(*b) + static_cast<double>(*a)) / 2.0;
}

// --------------------------------------------------------------------------
// Snapshot
// --------------------------------------------------------------------------
template <class SideMap>
void OrderBook::collect(const SideMap& book, std::size_t depth,
                        std::vector<Level>& out) {
    for (const auto& [price, fifo] : book) {       // map order == best -> worst
        Quantity q = 0;
        for (const auto& o : fifo) q += o.remaining;
        out.push_back(Level{price, q, fifo.size()});
        if (depth != 0 && out.size() >= depth) break;
    }
}

BookSnapshot OrderBook::snapshot(std::size_t depth) const {
    BookSnapshot s;
    collect(bids_, depth, s.bids);
    collect(asks_, depth, s.asks);
    return s;
}

} // namespace bourse