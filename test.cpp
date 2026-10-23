#include "MatchingEngine.hpp"
#include "FastIO.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

static int failures = 0;
static std::string captured_output;

static void captureOutput(const char* data, size_t size) {
    captured_output.append(data, size);
}

static void discardOutput(const char*, size_t) {}

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

static void test_parser_and_validation() {
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

    auto m6 = FastIO::parseLine("0,1,2,10,100");
    CHECK(m6.error_code == -3);

    auto m7 = FastIO::parseLine("0,1,0,10,100.5x");
    CHECK(m7.error_code == -3);

    auto m8 = FastIO::parseLine("0,1,0,10,100,extra");
    CHECK(m8.error_code == -1);

    auto m9 = FastIO::parseLine("0,1,0,10,100,");
    CHECK(m9.error_code == -1);

    auto m10 = FastIO::parseLine("1,42,extra");
    CHECK(m10.error_code == -1);

    auto m11 = FastIO::parseLine("1,42,");
    CHECK(m11.error_code == -1);

    auto m12 = FastIO::parseLine("0,1,0,0,100");
    CHECK(m12.error_code == -3);

    auto m13 = FastIO::parseLine("0,1,0,10,2000");
    CHECK(m13.error_code == -3);

    auto m14 = FastIO::parseLine("0,1,0,10,1999.9999");
    CHECK(m14.error_code == 0);
    CHECK(m14.price == 19999999);

    auto m15 = FastIO::parseLine("0,1,0,10,100.50000");
    CHECK(m15.error_code == -3);

    auto m16 = FastIO::parseLine("0,1,0,10,100.");
    CHECK(m16.error_code == -3);

    auto m17 = FastIO::parseLine("0,1,0,10,.5");
    CHECK(m17.error_code == -3);

    auto m18 = FastIO::parseLine("0,1,0,10,100.5.6");
    CHECK(m18.error_code == -3);

    auto m19 = FastIO::parseLine("0,1,0,10,100.5\r");
    CHECK(m19.error_code == 0);
    CHECK(m19.price == 1005000);

    auto m20 = FastIO::parseLine("0,1,0,10,18446744073709551616");
    CHECK(m20.error_code == -3);

    auto m21 = FastIO::parseLine("0,1,0,10,100 ");
    CHECK(m21.error_code == -3);
}

static void test_output_formatting() {
    FastIO::flush();
    captured_output.clear();
    FastIO::setSink(captureOutput);

    FastIO::formatTrade(10, 1005000);
    FastIO::formatFilled(7);
    FastIO::formatPartial(8, 3);
    FastIO::formatTrade(1, 1);
    FastIO::formatTrade(1, 10000);
    FastIO::formatTrade(1, 0);
    FastIO::flush();

    CHECK(captured_output ==
          "2,10,100.5\n3,7\n3,8,3\n2,1,0.0001\n2,1,1\n2,1,0\n");

    captured_output.clear();
    FastIO::setSink(discardOutput);
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

static void test_zero_quantity_is_ignored() {
    auto engine = std::make_unique<MatchingEngine>(100);
    engine->addOrder(1, Side::BUY, 0, 100);
    CHECK(!engine->isOrderActive(1));
}

static void test_node_reuse_when_pool_is_full() {
    auto engine = std::make_unique<MatchingEngine>(1);
    engine->addOrder(1, Side::BUY, 10, 100);
    CHECK(engine->isOrderActive(1));

    engine->addOrder(2, Side::BUY, 10, 200);
    CHECK(!engine->isOrderActive(2));

    engine->cancelOrder(1);
    engine->addOrder(3, Side::BUY, 10, 300);
    CHECK(engine->isOrderActive(3));
}

static void test_high_price_boundary() {
    auto engine = std::make_unique<MatchingEngine>(10);
    engine->addOrder(1, Side::SELL, 10, MAX_PRICE_RANGE - 1);
    CHECK(engine->isOrderActive(1));

    engine->addOrder(2, Side::BUY, 10, MAX_PRICE_RANGE - 1);
    CHECK(!engine->isOrderActive(1));
    CHECK(!engine->isOrderActive(2));
}

int main() {
    FastIO::setSink(discardOutput);

    test_memory_pool_and_intrusive_list();
    test_parser_and_validation();
    test_output_formatting();
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
    test_zero_quantity_is_ignored();
    test_node_reuse_when_pool_is_full();
    test_high_price_boundary();

    FastIO::flush();
    if (failures == 0) {
        std::printf("All tests passed successfully.\n");
        return 0;
    }

    std::printf("%d test(s) failed.\n", failures);
    return 1;
}
