//
// Bourse :: Python bindings  [PHASE 2 — not yet wired into the build]
//
// #include <pybind11/pybind11.h>
// #include <pybind11/stl.h>
// #include "bourse/order_book.hpp"
//
// namespace py = pybind11;
// using namespace bourse;
//
// PYBIND11_MODULE(_bourse, m) {
//     py::enum_<Side>(m, "Side")
//         .value("Buy", Side::Buy)
//         .value("Sell", Side::Sell);
//
//     py::class_<Trade>(m, "Trade")
//         .def_readonly("maker_id", &Trade::maker_id)
//         .def_readonly("taker_id", &Trade::taker_id)
//         .def_readonly("price", &Trade::price)
//         .def_readonly("qty", &Trade::qty)
//         .def_readonly("seq", &Trade::seq);
//
//     py::class_<OrderBook>(m, "OrderBook")
//         .def(py::init<>())
//         .def("submit_limit", &OrderBook::submit_limit)
//         .def("submit_market", &OrderBook::submit_market)
//         .def("cancel", &OrderBook::cancel)
//         .def("best_bid", &OrderBook::best_bid)
//         .def("best_ask", &OrderBook::best_ask)
//         .def("mid", &OrderBook::mid);
// }