#include "MatchingEngine.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <streambuf>

class NullBuffer : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
    int overflow(int c) override { return c; }
};

class ScopedCoutRedirect {
public:
    ScopedCoutRedirect() : previous_(std::cout.rdbuf(&null_)) {}
    ~ScopedCoutRedirect() {
        std::cout.flush();
        std::cout.rdbuf(previous_);
    }

private:
    NullBuffer null_;
    std::streambuf* previous_;
};

using Clock = std::chrono::steady_clock;

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

int main() {
    std::ios_base::sync_with_stdio(false);
    ScopedCoutRedirect redirect_output;

    std::printf("Matching engine benchmark (engine output suppressed)\n\n");
    constexpr int N = 5000;
    bench_add_only(Side::BUY, N, "add-only buys");
    bench_add_only(Side::SELL, N, "add-only sells");
    bench_match_buy(N);
    bench_match_sell(N);
    bench_cancel(N);

    return failures == 0 ? 0 : 1;
}
