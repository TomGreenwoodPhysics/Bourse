#pragma once
#include "bourse/types.hpp"
namespace bourse {

// A recorded mutating operation. An ordered log of Events fully determines a
// session: replaying it rebuilds identical book state and an identical tape.
enum class EventKind : std::uint8_t { Limit, Market, Cancel };

struct Event {
    EventKind kind;
    Side      side;   // Limit / Market
    Price     price;  // Limit
    Quantity  qty;    // Limit / Market
    OrderId   id;     // Cancel: target; Limit/Market: id assigned at submit (informational)
    Timestamp ts;
    friend bool operator==(const Event&, const Event&) = default;
};

} // namespace bourse