#pragma once
#include <cstdint>
namespace bourse {

using Price     = std::int64_t;
using Quantity  = std::int64_t;
using OrderId   = std::uint64_t;
using Seq       = std::uint64_t;   // engine-internal event sequence (time priority + log order)
using Timestamp = std::uint64_t;   // exogenous logical time supplied by the caller

enum class Side : std::uint8_t { Buy, Sell };
constexpr const char* to_string(Side s) noexcept { return s == Side::Buy ? "BUY" : "SELL"; }
constexpr Side opposite(Side s) noexcept { return s == Side::Buy ? Side::Sell : Side::Buy; }

struct Trade {
    OrderId   maker_id;
    OrderId   taker_id;
    Price     price;
    Quantity  qty;
    Side      taker_side;
    Seq       seq;
    Timestamp ts;          // logical time of execution = the aggressor's timestamp
    friend bool operator==(const Trade&, const Trade&) = default;
};

} // namespace bourse