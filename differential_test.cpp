#include "MatchingEngine.hpp"
#include "FastIO.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <set>

static void discardOutput(const char*, size_t) {}

struct RefOrder {
    uint64_t id;
    uint64_t quantity;
};

// Simple reference book used only by this test. It is intentionally
// straightforward rather than fast: std::map for price priority and
// std::deque for time priority.
struct RefBook {
    std::map<uint64_t, std::deque<RefOrder>> bids;
    std::map<uint64_t, std::deque<RefOrder>> asks;
    std::set<uint64_t> active;

    void add(uint64_t id, Side side, uint64_t quantity, uint64_t price) {
        if (side == Side::BUY) {
            while (quantity > 0 && !asks.empty()) {
                auto level = asks.begin();
                if (level->first > price) break;

                auto& queue = level->second;
                while (quantity > 0 && !queue.empty()) {
                    RefOrder& resting = queue.front();
                    const uint64_t trade_qty = std::min(quantity, resting.quantity);
                    quantity -= trade_qty;
                    resting.quantity -= trade_qty;
                    if (resting.quantity == 0) {
                        active.erase(resting.id);
                        queue.pop_front();
                    }
                }
                if (queue.empty()) asks.erase(level);
            }
            if (quantity > 0) {
                bids[price].push_back({id, quantity});
                active.insert(id);
            }
        } else {
            while (quantity > 0 && !bids.empty()) {
                auto level = std::prev(bids.end());
                if (level->first < price) break;

                auto& queue = level->second;
                while (quantity > 0 && !queue.empty()) {
                    RefOrder& resting = queue.front();
                    const uint64_t trade_qty = std::min(quantity, resting.quantity);
                    quantity -= trade_qty;
                    resting.quantity -= trade_qty;
                    if (resting.quantity == 0) {
                        active.erase(resting.id);
                        queue.pop_front();
                    }
                }
                if (queue.empty()) bids.erase(level);
            }
            if (quantity > 0) {
                asks[price].push_back({id, quantity});
                active.insert(id);
            }
        }
    }

    void cancel(uint64_t id) {
        if (active.erase(id) == 0) return;

        auto remove_from = [id](auto& book) {
            for (auto level = book.begin(); level != book.end();) {
                auto& queue = level->second;
                for (auto order = queue.begin(); order != queue.end(); ++order) {
                    if (order->id == id) {
                        queue.erase(order);
                        break;
                    }
                }
                if (queue.empty()) level = book.erase(level);
                else ++level;
            }
        };

        remove_from(bids);
        remove_from(asks);
    }
};

int main() {
    FastIO::setSink(discardOutput);

    constexpr int OPS = 20000;
    std::mt19937_64 rng(987654321);
    auto engine = std::make_unique<MatchingEngine>(OPS + 10);
    RefBook reference;
    uint64_t next_id = 1;
    int failures = 0;

    for (int op = 0; op < OPS; ++op) {
        const bool do_cancel = (rng() % 100) < 20 && !reference.active.empty();

        if (do_cancel) {
            auto victim = reference.active.begin();
            std::advance(victim, rng() % reference.active.size());
            const uint64_t id = *victim;
            reference.cancel(id);
            engine->cancelOrder(id);
        } else {
            const uint64_t id = next_id++;
            const Side side = (rng() & 1) ? Side::BUY : Side::SELL;
            const uint64_t quantity = 1 + (rng() % 10);
            const uint64_t price = (rng() & 1)
                ? (rng() % 200)
                : (rng() % MAX_PRICE_RANGE);
            reference.add(id, side, quantity, price);
            engine->addOrder(id, side, quantity, price);
        }

        if (op % 25 == 0 || op == OPS - 1) {
            for (uint64_t id = 1; id < next_id; ++id) {
                const bool expected = reference.active.count(id) != 0;
                const bool actual = engine->isOrderActive(id);
                if (expected != actual) {
                    std::printf("MISMATCH op=%d id=%llu expected=%d actual=%d\n",
                                op, static_cast<unsigned long long>(id),
                                expected ? 1 : 0, actual ? 1 : 0);
                    if (++failures > 20) {
                        FastIO::flush();
                        return 1;
                    }
                }
            }
        }
    }

    FastIO::flush();

    if (failures == 0) {
        std::printf("Differential test passed (%d operations, %llu order ids).\n",
                    OPS, static_cast<unsigned long long>(next_id - 1));
        return 0;
    }

    std::printf("Differential test failed with %d mismatch(es).\n", failures);
    return 1;
}
