//
// Bourse — timestamp + deterministic-replay tests (Phase 1 completion).
//
#include "testkit.hpp"
#include "bourse/order_book.hpp"

#include <random>
#include <vector>

using namespace bourse;

// Recording is opt-in; a default book logs nothing.
TEST_CASE(recording_off_by_default) {
    OrderBook b;
    b.submit_limit(Side::Buy, 99, 5);
    CHECK(b.recording() == false);
    CHECK(b.events().empty());
}

// The event log captures every mutating call, in order.
TEST_CASE(event_log_records_mutations) {
    OrderBook b(true);
    b.submit_limit(Side::Buy, 99, 5, 1);
    b.submit_market(Side::Sell, 2, 2);
    auto [id, _] = b.submit_limit(Side::Sell, 101, 4, 3);
    (void)_;
    b.cancel(id, 4);

    CHECK(b.events().size() == 4);
    CHECK(b.events()[0].kind == EventKind::Limit);
    CHECK(b.events()[1].kind == EventKind::Market);
    CHECK(b.events()[2].kind == EventKind::Limit);
    CHECK(b.events()[3].kind == EventKind::Cancel);
    CHECK(b.events()[3].id == id);
    CHECK(b.events()[3].ts == 4);
}

// A trade is stamped with the aggressor's (taker's) logical time.
TEST_CASE(trade_carries_taker_timestamp) {
    OrderBook b;
    b.submit_limit(Side::Sell, 100, 5, /*ts=*/10);   // maker rests at t=10
    auto ts = b.submit_market(Side::Buy, 3, /*ts=*/20);  // taker at t=20
    CHECK(ts.size() == 1);
    CHECK(ts.front().ts == 20);
}

// Property: replaying a recorded session reproduces the tape and book exactly.
TEST_CASE(replay_reproduces_session_exactly) {
    std::mt19937_64 rng(0xFEED);
    std::uniform_int_distribution<int>       side(0, 1);
    std::uniform_int_distribution<int>       kind(0, 9);
    std::uniform_int_distribution<Price>     price(95, 105);
    std::uniform_int_distribution<Quantity>  qty(1, 5);
    std::uniform_int_distribution<OrderId>   any_id(1, 300);

    OrderBook a(true);
    Timestamp clock = 0;
    for (int i = 0; i < 20000; ++i) {
        const Side s = side(rng) ? Side::Buy : Side::Sell;
        const Timestamp t = (clock += 1);
        const int k = kind(rng);
        if (k < 6)      a.submit_limit(s, price(rng), qty(rng), t);
        else if (k < 8) a.submit_market(s, qty(rng), t);
        else            a.cancel(any_id(rng), t);
    }

    OrderBook b = OrderBook::replay(a.events(), /*record=*/true);

    CHECK(a.trades()        == b.trades());
    CHECK(a.resting_dump()  == b.resting_dump());
    CHECK(a.sequence()      == b.sequence());
    CHECK(a.resting_orders()== b.resting_orders());
    CHECK(a.events()        == b.events());
}