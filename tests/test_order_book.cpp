//
// Bourse — order book correctness tests.
//
// Two flavours:
//   * Hand-written unit cases pinning the canonical matching rules.
//   * Property tests asserting invariants over randomised order flow.
//
#include "testkit.hpp"
#include "bourse/order_book.hpp"

#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

using namespace bourse;

namespace {

// Sum executed quantity across a trade vector.
Quantity filled(const std::vector<Trade>& ts) {
    Quantity q = 0;
    for (const auto& t : ts) q += t.qty;
    return q;
}

} // namespace

// ---------------------------------------------------------------------------
// Unit cases — the canonical rules every matching engine must obey
// ---------------------------------------------------------------------------

// A market buy lifts the LOWEST ask first (price priority).
TEST_CASE(market_buy_takes_best_price_first) {
    OrderBook b;
    b.submit_limit(Side::Sell, 101, 5);   // worse ask, rests first
    b.submit_limit(Side::Sell, 100, 5);   // better ask
    auto ts = b.submit_market(Side::Buy, 3);

    CHECK(ts.size() == 1);
    CHECK(filled(ts) == 3);
    CHECK(ts.front().price == 100);       // took 100, not 101
    CHECK(b.best_ask().value() == 100);   // 2 remain at 100
    CHECK(b.snapshot().asks.front().quantity == 2);
}

// At one price, the EARLIER order fills first (time priority / FIFO).
TEST_CASE(time_priority_is_fifo) {
    OrderBook b;
    auto [a_id, _a] = b.submit_limit(Side::Sell, 100, 5);  // arrived first
    auto [b_id, _b] = b.submit_limit(Side::Sell, 100, 5);  // arrived second
    (void)b_id;
    auto ts = b.submit_market(Side::Buy, 5);

    CHECK(ts.size() == 1);
    CHECK(ts.front().maker_id == a_id);   // the older order was hit
}

// A non-marketable limit just rests; it produces no trades.
TEST_CASE(passive_limit_rests) {
    OrderBook b;
    auto [id, ts] = b.submit_limit(Side::Buy, 99, 10);
    (void)id;
    CHECK(ts.empty());
    CHECK(b.best_bid().value() == 99);
    CHECK(b.resting_orders() == 1);
}

// A crossing limit executes immediately, then rests its residual at its limit.
TEST_CASE(crossing_limit_executes_then_rests_residual) {
    OrderBook b;
    b.submit_limit(Side::Sell, 100, 5);
    auto [id, ts] = b.submit_limit(Side::Buy, 101, 8);  // 8 wants in @<=101
    (void)id;

    CHECK(filled(ts) == 5);               // crossed the 5 @100
    CHECK(ts.front().price == 100);       // at the maker's price
    CHECK(!b.best_ask().has_value());     // ask side cleared
    CHECK(b.best_bid().value() == 101);   // residual 3 rests at 101
    CHECK(b.snapshot().bids.front().quantity == 3);
}

// A partial fill leaves the maker's remainder resting with its queue position.
TEST_CASE(partial_fill_leaves_remainder) {
    OrderBook b;
    b.submit_limit(Side::Sell, 100, 10);
    auto ts = b.submit_market(Side::Buy, 4);

    CHECK(filled(ts) == 4);
    CHECK(b.best_ask().value() == 100);
    CHECK(b.snapshot().asks.front().quantity == 6);
    CHECK(b.resting_orders() == 1);
}

// Cancel removes exactly the targeted order and nothing else.
TEST_CASE(cancel_removes_only_target) {
    OrderBook b;
    auto [id1, _1] = b.submit_limit(Side::Buy, 99, 5);
    auto [id2, _2] = b.submit_limit(Side::Buy, 99, 5);
    (void)id2;

    CHECK(b.snapshot().bids.front().order_count == 2);
    CHECK(b.cancel(id1) == true);
    CHECK(b.snapshot().bids.front().order_count == 1);
    CHECK(b.snapshot().bids.front().quantity == 5);
    CHECK(b.cancel(id1) == false);        // already gone
    CHECK(b.resting_orders() == 1);
}

// Cancelling the last order at a price removes the whole level.
TEST_CASE(cancel_empties_price_level) {
    OrderBook b;
    auto [id, _] = b.submit_limit(Side::Sell, 100, 5);
    (void)_;
    CHECK(b.best_ask().value() == 100);
    CHECK(b.cancel(id) == true);
    CHECK(!b.best_ask().has_value());
    CHECK(b.snapshot().asks.empty());
}

// Spread and mid derive correctly from top of book.
TEST_CASE(spread_and_mid) {
    OrderBook b;
    b.submit_limit(Side::Buy, 99, 1);
    b.submit_limit(Side::Sell, 101, 1);
    CHECK(b.spread().value() == 2);
    CHECK(b.mid().value() == 100.0);
}

// A market order against a thin book fills what it can, drops the rest.
TEST_CASE(market_order_partial_when_book_dry) {
    OrderBook b;
    b.submit_limit(Side::Sell, 100, 5);
    auto ts = b.submit_market(Side::Buy, 8);     // wants 8, only 5 available
    CHECK(filled(ts) == 5);
    CHECK(!b.best_ask().has_value());
    CHECK(b.resting_orders() == 0);              // nothing rested
}

// A limit sweeping multiple levels takes them in price order.
TEST_CASE(limit_sweeps_multiple_levels_in_order) {
    OrderBook b;
    b.submit_limit(Side::Sell, 102, 2);
    b.submit_limit(Side::Sell, 100, 2);
    b.submit_limit(Side::Sell, 101, 2);
    auto [id, ts] = b.submit_limit(Side::Buy, 102, 6);  // sweep all three
    (void)id;

    CHECK(ts.size() == 3);
    CHECK(ts[0].price == 100);    // best first
    CHECK(ts[1].price == 101);
    CHECK(ts[2].price == 102);
    CHECK(filled(ts) == 6);
    CHECK(b.resting_orders() == 0);
}

// ---------------------------------------------------------------------------
// Property tests — invariants over randomised order flow
// ---------------------------------------------------------------------------

// Invariant: the book never crosses. After any sequence of operations the
// best bid must be strictly below the best ask (otherwise a match was missed).
TEST_CASE(property_book_never_crosses) {
    std::mt19937_64 rng(0xC0FFEE);
    std::uniform_int_distribution<int>      side(0, 1);
    std::uniform_int_distribution<int>      kind(0, 9);   // 0..6 limit, 7..9 market
    std::uniform_int_distribution<Price>    price(95, 105);
    std::uniform_int_distribution<Quantity> qty(1, 5);

    OrderBook b;
    bool ever_crossed = false;
    for (int i = 0; i < 20000; ++i) {
        const Side s = side(rng) ? Side::Buy : Side::Sell;
        if (kind(rng) < 7) b.submit_limit(s, price(rng), qty(rng));
        else               b.submit_market(s, qty(rng));

        auto bb = b.best_bid(), ba = b.best_ask();
        if (bb && ba && *bb >= *ba) ever_crossed = true;
    }
    CHECK(!ever_crossed);
}

// Invariant: conservation of quantity. Across a session,
//   sum(submitted) == 2*sum(executed) + sum(resting) + sum(dropped).
// Every unit submitted is either traded, still resting, or was a dropped
// market residual — nothing is created or silently lost.
TEST_CASE(property_quantity_is_conserved) {
    std::mt19937_64 rng(0x1234);
    std::uniform_int_distribution<int>      side(0, 1);
    std::uniform_int_distribution<int>      kind(0, 9);
    std::uniform_int_distribution<Price>    price(95, 105);
    std::uniform_int_distribution<Quantity> qty(1, 5);

    OrderBook b;
    Quantity submitted = 0, executed = 0, dropped = 0;

    for (int i = 0; i < 20000; ++i) {
        const Side s = side(rng) ? Side::Buy : Side::Sell;
        const Quantity q = qty(rng);
        submitted += q;
        if (kind(rng) < 7) {
            auto [id, ts] = b.submit_limit(s, price(rng), q);
            (void)id;
            executed += filled(ts);
        } else {
            auto ts = b.submit_market(s, q);
            const Quantity f = filled(ts);
            executed += f;
            dropped  += (q - f);          // market residual that never rested
        }
    }

    // Resting quantity straight from the book.
    Quantity resting = 0;
    auto snap = b.snapshot();
    for (const auto& l : snap.bids) resting += l.quantity;
    for (const auto& l : snap.asks) resting += l.quantity;

    // Each trade consumes one unit of aggressor AND one unit of resting size,
    // and both were counted into `submitted` at their respective submit time,
    // so executed quantity enters the pool twice.
    CHECK(submitted == 2 * executed + resting + dropped);
}

// Invariant: the cancel index and the actual book agree. resting_orders()
// must equal the number of orders physically present across all levels.
TEST_CASE(property_index_matches_book) {
    std::mt19937_64 rng(0xABCD);
    std::uniform_int_distribution<int>      side(0, 1);
    std::uniform_int_distribution<int>      kind(0, 9);
    std::uniform_int_distribution<Price>    price(95, 105);
    std::uniform_int_distribution<Quantity> qty(1, 5);
    std::uniform_int_distribution<OrderId>  any_id(1, 200);

    OrderBook b;
    for (int i = 0; i < 20000; ++i) {
        const int k = kind(rng);
        const Side s = side(rng) ? Side::Buy : Side::Sell;
        if (k < 6)      b.submit_limit(s, price(rng), qty(rng));
        else if (k < 8) b.submit_market(s, qty(rng));
        else            b.cancel(any_id(rng));   // mostly stale ids: must be safe
    }

    std::size_t counted = 0;
    auto snap = b.snapshot();
    for (const auto& l : snap.bids) counted += l.order_count;
    for (const auto& l : snap.asks) counted += l.order_count;
    CHECK(counted == b.resting_orders());
}