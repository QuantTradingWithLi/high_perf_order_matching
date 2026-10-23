#include "MatchingEngine.hpp"
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <random>
#include <set>

static int failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,    \
                         __LINE__, #expr);                                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_edges() {
    PriceBitmap empty(0);
    CHECK(empty.find_first() == PriceBitmap::npos);
    CHECK(empty.find_next(0) == PriceBitmap::npos);
    CHECK(empty.find_prev(0) == PriceBitmap::npos);

    PriceBitmap bitmap(130);
    bitmap.set(0);
    bitmap.set(63);
    bitmap.set(64);
    bitmap.set(129);

    CHECK(bitmap.test(0));
    CHECK(bitmap.test(63));
    CHECK(bitmap.test(64));
    CHECK(bitmap.test(129));
    CHECK(!bitmap.test(1));
    CHECK(!bitmap.test(128));

    CHECK(bitmap.find_first() == 0);
    CHECK(bitmap.find_at_or_after(1) == 63);
    CHECK(bitmap.find_at_or_after(64) == 64);
    CHECK(bitmap.find_at_or_after(65) == 129);
    CHECK(bitmap.find_at_or_after(130) == PriceBitmap::npos);

    CHECK(bitmap.find_next(0) == 63);
    CHECK(bitmap.find_next(63) == 64);
    CHECK(bitmap.find_next(64) == 129);
    CHECK(bitmap.find_next(129) == PriceBitmap::npos);

    CHECK(bitmap.find_prev(130) == 129);
    CHECK(bitmap.find_prev(129) == 64);
    CHECK(bitmap.find_prev(64) == 63);
    CHECK(bitmap.find_prev(63) == 0);
    CHECK(bitmap.find_prev(0) == PriceBitmap::npos);

    bitmap.reset(0);
    bitmap.reset(64);
    CHECK(bitmap.find_first() == 63);
    CHECK(bitmap.find_prev(130) == 129);
    CHECK(bitmap.find_prev(129) == 63);
}

static void test_randomized() {
    constexpr size_t PRICE_DOMAIN = 20000000;
    PriceBitmap bitmap(PRICE_DOMAIN);
    std::set<size_t> expected;
    std::mt19937_64 rng(123456789);

    for (int i = 0; i < 50000; ++i) {
        const size_t price = rng() % PRICE_DOMAIN;
        if (rng() & 1) {
            bitmap.set(price);
            expected.insert(price);
        } else {
            bitmap.reset(price);
            expected.erase(price);
        }
    }

    CHECK(bitmap.find_first() ==
          (expected.empty() ? PriceBitmap::npos : *expected.begin()));

    for (int i = 0; i < 20000; ++i) {
        const size_t price = rng() % PRICE_DOMAIN;

        auto at_or_after = expected.lower_bound(price);
        CHECK(bitmap.find_at_or_after(price) ==
              (at_or_after == expected.end() ? PriceBitmap::npos
                                             : *at_or_after));

        auto after = expected.upper_bound(price);
        CHECK(bitmap.find_next(price) ==
              (after == expected.end() ? PriceBitmap::npos : *after));

        auto before = expected.lower_bound(price);
        CHECK(bitmap.find_prev(price) ==
              (before == expected.begin() ? PriceBitmap::npos
                                          : *std::prev(before)));
    }
}

int main() {
    test_edges();
    test_randomized();

    if (failures == 0) {
        std::printf("PriceBitmap tests passed.\n");
        return 0;
    }

    std::printf("PriceBitmap tests failed with %d failure(s).\n", failures);
    return 1;
}
