#include "MatchingEngine.hpp"
#include <chrono>
#include <cstdio>

int main() {
    auto bench = [](Side s, const char* tag) {
        MatchingEngine eng(1'000'000);                 // empty book
        auto t0 = std::chrono::steady_clock::now();
        for (uint64_t i = 0; i < 1000; ++i)
            eng.addOrder(i + 1, s, /*qty=*/10, /*price=*/100000);  // all just rest
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        printf("%-34s : 1000 orders took %8.1f ms (%.3f ms/order)\n", tag, ms, ms / 1000.0);
    };
    bench(Side::BUY,  "BUY  (scans [0, price])");
    bench(Side::SELL, "SELL (scans [price, 20,000,000))");
}