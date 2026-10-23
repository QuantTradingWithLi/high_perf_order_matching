#include "SPSCQueue.hpp"
#include "FastIO.hpp"

#include <cstdint>
#include <cstdio>
#include <thread>

static int failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,    \
                         __LINE__, #expr);                                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_single_threaded_ring() {
    SPSCQueue<int, 4> queue;
    int value = 0;

    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK(!queue.push(4));

    CHECK(queue.pop(value) && value == 1);
    CHECK(queue.pop(value) && value == 2);
    CHECK(queue.push(4));
    CHECK(queue.push(5));
    CHECK(!queue.push(6));

    CHECK(queue.pop(value) && value == 3);
    CHECK(queue.pop(value) && value == 4);
    CHECK(queue.pop(value) && value == 5);
    CHECK(!queue.pop(value));
}

static void test_pod_messages() {
    SPSCQueue<ParsedMessage, 8> queue;
    ParsedMessage message;
    message.id = 42;
    message.quantity = 10;
    message.price = 1005000;
    message.side = Side::SELL;
    message.error_code = 0;

    CHECK(queue.push(message));
    CHECK(queue.pop(message));
    CHECK(message.id == 42);
    CHECK(message.quantity == 10);
    CHECK(message.price == 1005000);
    CHECK(message.side == Side::SELL);
}

static void test_two_threads() {
    constexpr int MESSAGES = 100000;
    SPSCQueue<int, 1024> queue;
    std::thread producer([&] {
        for (int i = 0; i < MESSAGES; ++i) {
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    int expected = 0;
    int value = 0;
    while (expected < MESSAGES) {
        if (queue.pop(value)) {
            CHECK(value == expected);
            ++expected;
        } else {
            std::this_thread::yield();
        }
    }

    producer.join();
    CHECK(!queue.pop(value));
}

int main() {
    test_single_threaded_ring();
    test_pod_messages();
    test_two_threads();

    if (failures == 0) {
        std::printf("SPSCQueue tests passed.\n");
        return 0;
    }

    std::printf("SPSCQueue tests failed with %d failure(s).\n", failures);
    return 1;
}
