#include "MatchingEngine.hpp"
#include "FastIO.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

using Clock = std::chrono::steady_clock;

static void discardOutput(const char*, size_t) {}

template <typename F>
static double time_ms(F&& fn) {
    const auto start = Clock::now();
    fn();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static size_t count_active(const MatchingEngine& engine, int max_id) {
    size_t active = 0;
    for (int id = 1; id <= max_id; ++id) {
        active += engine.isOrderActive(static_cast<uint64_t>(id)) ? 1 : 0;
    }
    return active;
}

static int failures = 0;

static void report(const char* label, int n, double ms, size_t remaining,
                   size_t expected) {
    std::printf("%-34s n=%d  %8.3f ms  %8.3f us/order", label, n, ms,
                ms * 1000.0 / n);
    if (remaining != expected) {
        std::printf("  remaining=%zu expected=%zu FAIL", remaining, expected);
        ++failures;
    }
    std::printf("\n");
}

static void bench_add_only(Side side, int n, const char* label) {
    auto engine = std::make_unique<MatchingEngine>(static_cast<size_t>(n) + 10);
    const double ms = time_ms([&] {
        for (int i = 0; i < n; ++i) {
            engine->addOrder(static_cast<uint64_t>(i + 1), side, 10, 1000000);
        }
    });
    report(label, n, ms, count_active(*engine, n), static_cast<size_t>(n));
}

static void bench_match_buy(int n) {
    auto engine = std::make_unique<MatchingEngine>(static_cast<size_t>(2 * n) + 10);
    for (int i = 0; i < n; ++i) {
        engine->addOrder(static_cast<uint64_t>(i + 1), Side::SELL, 10,
                         1000000 + static_cast<uint64_t>(i));
    }

    const double ms = time_ms([&] {
        for (int i = 0; i < n; ++i) {
            engine->addOrder(static_cast<uint64_t>(n + i + 1), Side::BUY, 10,
                             2000000);
        }
    });
    report("buy sweeps ask levels", n, ms, count_active(*engine, 2 * n), 0);
}

static void bench_match_sell(int n) {
    auto engine = std::make_unique<MatchingEngine>(static_cast<size_t>(2 * n) + 10);
    for (int i = 0; i < n; ++i) {
        engine->addOrder(static_cast<uint64_t>(i + 1), Side::BUY, 10,
                         1000000 - static_cast<uint64_t>(i));
    }

    const double ms = time_ms([&] {
        for (int i = 0; i < n; ++i) {
            engine->addOrder(static_cast<uint64_t>(n + i + 1), Side::SELL, 10, 0);
        }
    });
    report("sell sweeps bid levels", n, ms, count_active(*engine, 2 * n), 0);
}

static void bench_cancel(int n) {
    auto engine = std::make_unique<MatchingEngine>(static_cast<size_t>(n) + 10);
    for (int i = 0; i < n; ++i) {
        engine->addOrder(static_cast<uint64_t>(i + 1), Side::BUY, 10, 1000000);
    }

    const double ms = time_ms([&] {
        for (int i = 0; i < n; ++i) {
            engine->cancelOrder(static_cast<uint64_t>(i + 1));
        }
    });
    report("cancel resting orders", n, ms, count_active(*engine, n), 0);
}

static void bench_construct(size_t max_orders) {
    std::unique_ptr<MatchingEngine> engine;
    const double ms = time_ms([&] {
        engine = std::make_unique<MatchingEngine>(max_orders);
    });
    std::printf("%-34s max_orders=%zu  %8.3f ms\n", "construct engine",
                max_orders, ms);
    engine.reset();
}

int main() {
    FastIO::setSink(discardOutput);

    std::printf("Matching engine benchmark (engine output suppressed)\n");
    std::printf("sizeof(OrderNode)=%zu  sizeof(PriceLevel)=%zu  "
                "sizeof(MatchingEngine)=%zu\n\n",
                sizeof(OrderNode), sizeof(PriceLevel), sizeof(MatchingEngine));

    constexpr int N = 5000;
    bench_construct(1000000);
    std::printf("\n");

    bench_add_only(Side::BUY, N, "add-only buys");
    bench_add_only(Side::SELL, N, "add-only sells");
    bench_match_buy(N);
    bench_match_sell(N);
    bench_cancel(N);

    FastIO::flush();
    return failures == 0 ? 0 : 1;
}
