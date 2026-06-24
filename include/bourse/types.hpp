#pragma once
//
// Bourse — core value types.
//
// Prices are integer ticks, never floating point. Real exchanges quote on a
// discrete tick grid, and integer arithmetic keeps matching exact and
// deterministic (no float equality hazards in price-level comparisons).
//
#include <cstdint>

namespace bourse {

using Price    = std::int64_t;   // price in integer ticks
using Quantity = std::int64_t;   // units (shares / contracts / lots)
using OrderId  = std::uint64_t;  // engine-assigned, monotonic, never reused
using Seq      = std::uint64_t;  // global event sequence: time priority + log ordering

enum class Side : std::uint8_t { Buy, Sell };

constexpr const char* to_string(Side s) noexcept {
    return s == Side::Buy ? "BUY" : "SELL";
}

constexpr Side opposite(Side s) noexcept {
    return s == Side::Buy ? Side::Sell : Side::Buy;
}

// A single execution. Price is always the resting (maker) order's price —
// the aggressor pays the price that was already on the book (price improvement
// accrues to the taker), which is standard continuous-auction behaviour.
struct Trade {
    OrderId  maker_id;    // resting order that was hit
    OrderId  taker_id;    // incoming aggressor
    Price    price;       // execution price = maker's resting price
    Quantity qty;         // executed quantity
    Side     taker_side;  // direction of the aggressor
    Seq      seq;         // event sequence number
};

} // namespace bourse