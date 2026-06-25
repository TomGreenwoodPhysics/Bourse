#include "bourse/order_book.hpp"

#include <algorithm>
#include <iterator>

namespace bourse {

OrderBook::OrderBook(bool record_events) : record_(record_events) {}

template <class OppMap>
void OrderBook::match(OppMap& opp, Side taker_side, OrderId taker_id,
                      Price limit, bool is_market, Timestamp taker_ts,
                      Quantity& remaining, std::vector<Trade>& out) {
    while (remaining > 0 && !opp.empty()) {
        auto level_it = opp.begin();
        const Price level_price = level_it->first;

        const bool marketable =
            is_market ||
            (taker_side == Side::Buy ? level_price <= limit : level_price >= limit);
        if (!marketable) break;

        LevelList& fifo = level_it->second;
        while (remaining > 0 && !fifo.empty()) {
            Order& maker = fifo.front();
            const Quantity fill = std::min(remaining, maker.remaining);

            Trade t{maker.id, taker_id, level_price, fill,
                    taker_side, next_seq(), taker_ts};
            out.push_back(t);
            trades_.push_back(t);

            remaining       -= fill;
            maker.remaining -= fill;

            if (maker.remaining == 0) { index_.erase(maker.id); fifo.pop_front(); }
        }
        if (fifo.empty()) opp.erase(level_it);
    }
}

template <class SideMap>
void OrderBook::rest(SideMap& book, Side side, OrderId id,
                     Price price, Quantity qty, Timestamp ts) {
    LevelList& fifo = book[price];
    fifo.push_back(Order{id, side, price, qty, next_seq(), ts});
    index_.emplace(id, Locator{side, price, std::prev(fifo.end())});
}

std::pair<OrderId, std::vector<Trade>>
OrderBook::submit_limit(Side side, Price price, Quantity qty, Timestamp ts) {
    const OrderId id = next_id_++;
    if (record_) events_.push_back(Event{EventKind::Limit, side, price, qty, id, ts});

    std::vector<Trade> out;
    if (qty <= 0) return {id, out};

    Quantity remaining = qty;
    if (side == Side::Buy) match(asks_, Side::Buy,  id, price, false, ts, remaining, out);
    else                   match(bids_, Side::Sell, id, price, false, ts, remaining, out);

    if (remaining > 0) {
        if (side == Side::Buy) rest(bids_, side, id, price, remaining, ts);
        else                   rest(asks_, side, id, price, remaining, ts);
    }
    return {id, out};
}

std::vector<Trade>
OrderBook::submit_market(Side side, Quantity qty, Timestamp ts) {
    const OrderId id = next_id_++;
    if (record_) events_.push_back(Event{EventKind::Market, side, 0, qty, id, ts});

    std::vector<Trade> out;
    if (qty <= 0) return out;

    Quantity remaining = qty;
    if (side == Side::Buy) match(asks_, Side::Buy,  id, 0, true, ts, remaining, out);
    else                   match(bids_, Side::Sell, id, 0, true, ts, remaining, out);
    return out;
}

bool OrderBook::cancel(OrderId id, Timestamp ts) {
    if (record_) events_.push_back(Event{EventKind::Cancel, Side::Buy, 0, 0, id, ts});

    auto idx = index_.find(id);
    if (idx == index_.end()) return false;

    const Locator loc = idx->second;
    if (loc.side == Side::Buy) {
        auto lvl = bids_.find(loc.price);
        if (lvl != bids_.end()) { lvl->second.erase(loc.it); if (lvl->second.empty()) bids_.erase(lvl); }
    } else {
        auto lvl = asks_.find(loc.price);
        if (lvl != asks_.end()) { lvl->second.erase(loc.it); if (lvl->second.empty()) asks_.erase(lvl); }
    }
    index_.erase(idx);
    return true;
}

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

template <class SideMap>
void OrderBook::collect(const SideMap& book, std::size_t depth, std::vector<Level>& out) {
    for (const auto& [price, fifo] : book) {
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

std::vector<Order> OrderBook::resting_dump() const {
    std::vector<Order> v;
    for (const auto& [p, fifo] : bids_) for (const auto& o : fifo) v.push_back(o);
    for (const auto& [p, fifo] : asks_) for (const auto& o : fifo) v.push_back(o);
    return v;
}

OrderBook OrderBook::replay(const std::vector<Event>& log, bool record_events) {
    OrderBook b(record_events);
    for (const auto& e : log) {
        switch (e.kind) {
            case EventKind::Limit:  b.submit_limit(e.side, e.price, e.qty, e.ts); break;
            case EventKind::Market: b.submit_market(e.side, e.qty, e.ts);         break;
            case EventKind::Cancel: b.cancel(e.id, e.ts);                         break;
        }
    }
    return b;
}

} // namespace bourse