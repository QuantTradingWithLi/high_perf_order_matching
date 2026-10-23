#include "FlatHashMap.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <unordered_map>

static int failures = 0;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__,    \
                         __LINE__, #expr);                                   \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_basic_operations() {
    FlatHashMap map(4);

    CHECK(map.empty());
    CHECK(map.insert(10, 1));
    CHECK(map.insert(20, 2));
    CHECK(map.insert(30, 3));
    CHECK(map.size() == 3);

    const uint32_t* value = map.find(20);
    CHECK(value != nullptr && *value == 2);
    CHECK(map.find(999) == nullptr);

    CHECK(!map.insert(20, 99));
    CHECK(map.find(20) != nullptr && *map.find(20) == 2);
    CHECK(!map.insert(40, FlatHashMap::NIL));

    CHECK(map.erase(20));
    CHECK(!map.erase(20));
    CHECK(map.size() == 2);
    CHECK(map.find(20) == nullptr);

    CHECK(map.insert(20, 4));
    CHECK(map.size() == 3);
    CHECK(map.find(20) != nullptr && *map.find(20) == 4);

    CHECK(map.erase(10));
    CHECK(map.erase(30));
    CHECK(map.erase(20));
    CHECK(map.empty());
}

static void test_growth_and_tombstones() {
    FlatHashMap map(1);

    for (uint64_t i = 0; i < 2000; ++i) {
        CHECK(map.insert(i, static_cast<uint32_t>(i + 1)));
    }
    CHECK(map.size() == 2000);

    for (uint64_t i = 0; i < 2000; i += 2) {
        CHECK(map.erase(i));
    }
    CHECK(map.size() == 1000);

    for (uint64_t i = 0; i < 2000; i += 2) {
        CHECK(map.insert(i, static_cast<uint32_t>(i + 100)));
    }
    CHECK(map.size() == 2000);

    for (uint64_t i = 0; i < 2000; ++i) {
        const uint32_t* value = map.find(i);
        CHECK(value != nullptr);
        if (value != nullptr) {
            const uint32_t expected =
                i % 2 == 0 ? static_cast<uint32_t>(i + 100)
                           : static_cast<uint32_t>(i + 1);
            CHECK(*value == expected);
        }
    }
}

static void test_randomized_against_reference() {
    constexpr int OPERATIONS = 100000;
    std::mt19937_64 rng(123456789);
    FlatHashMap map(20000);
    std::unordered_map<uint64_t, uint32_t> reference;

    for (int op = 0; op < OPERATIONS; ++op) {
        const uint64_t key = rng() % 50000;
        const int action = static_cast<int>(rng() % 3);

        if (action == 0) {
            const uint32_t value = static_cast<uint32_t>(op + 1);
            const bool expected = reference.emplace(key, value).second;
            const bool actual = map.insert(key, value);
            CHECK(actual == expected);
        } else if (action == 1) {
            CHECK(map.erase(key) == (reference.erase(key) != 0));
        } else {
            const auto it = reference.find(key);
            const uint32_t* value = map.find(key);
            CHECK((it == reference.end()) == (value == nullptr));
            if (it != reference.end() && value != nullptr) {
                CHECK(*value == it->second);
            }
        }
    }

    CHECK(map.size() == reference.size());
    for (const auto& [key, value] : reference) {
        const uint32_t* found = map.find(key);
        CHECK(found != nullptr);
        if (found != nullptr) CHECK(*found == value);
    }
}

int main() {
    test_basic_operations();
    test_growth_and_tombstones();
    test_randomized_against_reference();

    if (failures == 0) {
        std::printf("FlatHashMap tests passed.\n");
        return 0;
    }

    std::printf("FlatHashMap tests failed with %d failure(s).\n", failures);
    return 1;
}
