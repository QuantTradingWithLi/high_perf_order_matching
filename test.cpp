#include "MatchingEngine.hpp"
#include "FastIO.hpp"
#include <cstdint>
#include <cstdio>
#include <memory>

static int failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,    \
                         __LINE__, #expr);                                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_memory_pool_and_intrusive_list() {
    auto engine = std::make_unique<MatchingEngine>(100);

    engine->addOrder(1, Side::BUY, 10, 1000000);
    CHECK(engine->isOrderActive(1));

    engine->addOrder(2, Side::SELL, 10, 1000000);
    CHECK(!engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));
}

static void test_parser_negative_errors() {
    auto m1 = FastIO::parseLine("BADMESSAGE");
    CHECK(m1.error_code == -3);

    auto m2 = FastIO::parseLine("99,123,0,9,100");
    CHECK(m2.error_code == -2);

    auto m3 = FastIO::parseLine("0,ABC,0,9,100");
    CHECK(m3.error_code == -3);

    auto m4 = FastIO::parseLine("0,1,0,10,100.5");
    CHECK(m4.error_code == 0);
    CHECK(m4.id == 1);
    CHECK(m4.side == Side::BUY);
    CHECK(m4.quantity == 10);
    CHECK(m4.price == 1005000);

    auto m5 = FastIO::parseLine("1,42");
    CHECK(m5.error_code == 1);
    CHECK(m5.id == 42);
}

static void test_sell_sweeps_multiple_price_levels() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 10, 1000000);
    engine->addOrder(2, Side::BUY, 10, 900000);
    engine->addOrder(3, Side::SELL, 20, 900000);

    CHECK(!engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));
}

static void test_sell_sweeps_after_partial_top_level() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 3, 1000000);
    engine->addOrder(2, Side::BUY, 7, 900000);
    engine->addOrder(3, Side::SELL, 10, 900000);

    CHECK(!engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));
}

static void test_best_bid_after_fill() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 5, 1000000);
    engine->addOrder(2, Side::BUY, 5, 900000);

    engine->addOrder(3, Side::SELL, 5, 900000);
    CHECK(!engine->isOrderActive(1));
    CHECK(engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));

    engine->addOrder(4, Side::SELL, 5, 900000);
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(4));
}

static void test_best_ask_after_fill() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::SELL, 5, 900000);
    engine->addOrder(2, Side::SELL, 5, 1000000);

    engine->addOrder(3, Side::BUY, 5, 900000);
    CHECK(!engine->isOrderActive(1));
    CHECK(engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));

    engine->addOrder(4, Side::BUY, 5, 1000000);
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(4));
}

static void test_price_time_priority() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 5, 100);
    engine->addOrder(2, Side::BUY, 5, 100);
    engine->addOrder(3, Side::SELL, 7, 100);

    CHECK(!engine->isOrderActive(1));
    CHECK(engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));

    engine->addOrder(4, Side::SELL, 3, 100);
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(4));
}

static void test_best_ask_after_cancel() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::SELL, 5, 900000);
    engine->addOrder(2, Side::SELL, 5, 1000000);
    engine->cancelOrder(1);

    engine->addOrder(3, Side::BUY, 5, 1000000);
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));
}

static void test_best_bid_after_cancel() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 5, 1000000);
    engine->addOrder(2, Side::BUY, 5, 900000);
    engine->cancelOrder(1);

    engine->addOrder(3, Side::SELL, 5, 900000);
    CHECK(!engine->isOrderActive(2));
    CHECK(!engine->isOrderActive(3));
}

static void test_partial_fill_and_cancel() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 10, 100);
    engine->addOrder(2, Side::SELL, 4, 100);

    CHECK(engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));

    engine->cancelOrder(1);
    CHECK(!engine->isOrderActive(1));
}

static void test_zero_price() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 5, 0);
    engine->addOrder(2, Side::SELL, 5, 0);
    CHECK(!engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));
}

static void test_duplicate_id_is_ignored() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 10, 100);
    engine->addOrder(1, Side::BUY, 10, 200);
    engine->cancelOrder(1);
    CHECK(!engine->isOrderActive(1));

    engine->addOrder(2, Side::SELL, 10, 100);
    CHECK(engine->isOrderActive(2));
}

static void test_invalid_side_and_price_are_ignored() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, static_cast<Side>(2), 10, 100);
    CHECK(!engine->isOrderActive(1));

    engine->addOrder(2, Side::BUY, 10, MAX_PRICE_RANGE);
    CHECK(!engine->isOrderActive(2));
}

static void test_cancel_unknown_order_is_noop() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 10, 100);
    engine->cancelOrder(999999);
    CHECK(engine->isOrderActive(1));
}

int main() {
    test_memory_pool_and_intrusive_list();
    test_parser_negative_errors();
    test_sell_sweeps_multiple_price_levels();
    test_sell_sweeps_after_partial_top_level();
    test_best_bid_after_fill();
    test_best_ask_after_fill();
    test_price_time_priority();
    test_best_ask_after_cancel();
    test_best_bid_after_cancel();
    test_partial_fill_and_cancel();
    test_zero_price();
    test_duplicate_id_is_ignored();
    test_invalid_side_and_price_are_ignored();
    test_cancel_unknown_order_is_noop();

    if (failures == 0) {
        std::printf("All tests passed successfully.\n");
        return 0;
    }

    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
