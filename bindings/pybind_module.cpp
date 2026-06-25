//
// Bourse :: Python bindings — exposes the C++ OrderBook to Python.
//
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>      // vector / pair / optional <-> Python
#include <pybind11/operators.h>

#include <string>

#include "bourse/order_book.hpp"

namespace py = pybind11;
using namespace bourse;

PYBIND11_MODULE(_bourse, m) {
    m.doc() = "Bourse — C++ limit order book matching engine (pybind11 bindings).";

    py::enum_<Side>(m, "Side")
        .value("Buy",  Side::Buy)
        .value("Sell", Side::Sell);

    py::enum_<EventKind>(m, "EventKind")
        .value("Limit",  EventKind::Limit)
        .value("Market", EventKind::Market)
        .value("Cancel", EventKind::Cancel);

    py::class_<Trade>(m, "Trade")
        .def_readonly("maker_id",   &Trade::maker_id)
        .def_readonly("taker_id",   &Trade::taker_id)
        .def_readonly("price",      &Trade::price)
        .def_readonly("qty",        &Trade::qty)
        .def_readonly("taker_side", &Trade::taker_side)
        .def_readonly("seq",        &Trade::seq)
        .def_readonly("ts",         &Trade::ts)
        .def(py::self == py::self)
        .def("__repr__", [](const Trade& t) {
            return "<Trade seq=" + std::to_string(t.seq) +
                   " px="    + std::to_string(t.price) +
                   " qty="   + std::to_string(t.qty) +
                   " maker=" + std::to_string(t.maker_id) +
                   " taker=" + std::to_string(t.taker_id) +
                   " ts="    + std::to_string(t.ts) + ">";
        });

    py::class_<Order>(m, "Order")
        .def_readonly("id",        &Order::id)
        .def_readonly("side",      &Order::side)
        .def_readonly("price",     &Order::price)
        .def_readonly("remaining", &Order::remaining)
        .def_readonly("seq",       &Order::seq)
        .def_readonly("ts",        &Order::ts)
        .def(py::self == py::self);

    py::class_<Event>(m, "Event")
        .def_readonly("kind",  &Event::kind)
        .def_readonly("side",  &Event::side)
        .def_readonly("price", &Event::price)
        .def_readonly("qty",   &Event::qty)
        .def_readonly("id",    &Event::id)
        .def_readonly("ts",    &Event::ts)
        .def(py::self == py::self);

    py::class_<Level>(m, "Level")
        .def_readonly("price",       &Level::price)
        .def_readonly("quantity",    &Level::quantity)
        .def_readonly("order_count", &Level::order_count)
        .def("__repr__", [](const Level& l) {
            return "<Level px=" + std::to_string(l.price) +
                   " qty="      + std::to_string(l.quantity) +
                   " n="        + std::to_string(l.order_count) + ">";
        });

    py::class_<BookSnapshot>(m, "BookSnapshot")
        .def_readonly("bids", &BookSnapshot::bids)
        .def_readonly("asks", &BookSnapshot::asks);

    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<bool>(), py::arg("record_events") = false)
        .def("submit_limit",  &OrderBook::submit_limit,
             py::arg("side"), py::arg("price"), py::arg("qty"), py::arg("ts") = 0)
        .def("submit_market", &OrderBook::submit_market,
             py::arg("side"), py::arg("qty"), py::arg("ts") = 0)
        .def("cancel",         &OrderBook::cancel, py::arg("order_id"), py::arg("ts") = 0)
        .def("best_bid",       &OrderBook::best_bid)
        .def("best_ask",       &OrderBook::best_ask)
        .def("spread",         &OrderBook::spread)
        .def("mid",            &OrderBook::mid)
        .def("snapshot",       &OrderBook::snapshot, py::arg("depth") = 0)
        .def("trades",         &OrderBook::trades)
        .def("sequence",       &OrderBook::sequence)
        .def("resting_orders", &OrderBook::resting_orders)
        .def("recording",      &OrderBook::recording)
        .def("events",         &OrderBook::events)
        .def("resting_dump",   &OrderBook::resting_dump)
        .def_static("replay",  &OrderBook::replay,
                    py::arg("log"), py::arg("record_events") = false);
}