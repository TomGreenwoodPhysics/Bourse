//
// Bourse — throughput micro-benchmark.
//
// Indicative only: single translation unit, whatever core the machine gives
// us. The point is a repeatable number to track as the engine is optimised,
// not an absolute HFT figure.
//
#include "bourse/order_book.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace bourse;

int main(int argc, char** argv) {
    const long N = (argc > 1) ? std::atol(argv[1]) : 5'000'000;

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int>      side(0, 1);
    std::uniform_int_distribution<int>      kind(0, 9);
    std::uniform_int_distribution<Price>    price(9900, 10100);
    std::uniform_int_distribution<Quantity> qty(1, 10);

    OrderBook b;
    const auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < N; ++i) {
        const Side s = side(rng) ? Side::Buy : Side::Sell;
        if (kind(rng) < 8) b.submit_limit(s, price(rng), qty(rng));
        else               b.submit_market(s, qty(rng));
    }
    const auto t1 = std::chrono::steady_clock::now();

    const double secs = std::chrono::duration<double>(t1 - t0).count();
    std::printf("submitted %ld orders in %.3f s\n", N, secs);
    std::printf("throughput: %.2f M orders/sec\n", (N / secs) / 1e6);
    std::printf("avg latency: %.1f ns/order\n", (secs / N) * 1e9);
    std::printf("trades generated: %zu | resting: %zu\n",
                b.trades().size(), b.resting_orders());
    return 0;
}