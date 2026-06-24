#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <vector>
#include <unordered_map>
#include <memory>
#include <bitset>
#include <algorithm>
#include "OrderNode.hpp"

struct PriceLevel {
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;

    void push_back(OrderNode* node) {
        if (!head) { head = tail = node; node->next = node->prev = nullptr; }
        else { tail->next = node; node->prev = tail; node->next = nullptr; tail = node; }
    }
    void remove(OrderNode* node) {
        if (node->prev) node->prev->next = node->next; else head = node->next;
        if (node->next) node->next->prev = node->prev; else tail = node->prev;
    }
    bool empty() const { return head == nullptr; }
};

static constexpr size_t MAX_PRICE_RANGE = 20000000;

class MatchingEngine {
private:
    std::vector<OrderNode> orderPool;
    std::vector<OrderNode*> freeList;
    std::unordered_map<uint64_t, OrderNode*> directory;

    // Unique pointers for heap allocation to avoid stack overflow
    std::unique_ptr<PriceLevel[]> bids;
    std::unique_ptr<PriceLevel[]> asks;
    std::unique_ptr<std::bitset<MAX_PRICE_RANGE>> active_bids;
    std::unique_ptr<std::bitset<MAX_PRICE_RANGE>> active_asks;

    OrderNode* allocateNode() noexcept;
    void deallocateNode(OrderNode* node) noexcept;
    void executeTrade(OrderNode* agg, OrderNode* resting, uint64_t trade_qty) noexcept;

    // Generic processor to handle Buy/Sell symmetry
    template <Side S>
    void processOrder(OrderNode* agg) noexcept;

public:
    explicit MatchingEngine(size_t max_orders = 1000000) noexcept;
    void addOrder(uint64_t id, Side side, uint64_t quantity, uint64_t price) noexcept;
    void cancelOrder(uint64_t id) noexcept;
    bool isOrderActive(uint64_t id) const noexcept;
};

template <Side S>
void MatchingEngine::processOrder(OrderNode* agg) noexcept {
    auto& active_levels = (S == Side::BUY) ? *active_asks : *active_bids;
    auto& book = (S == Side::BUY) ? asks : bids;

    if constexpr (S == Side::BUY) {
        for (uint64_t p = 0; p <= agg->price && p < MAX_PRICE_RANGE; ++p) {
            if (agg->quantity == 0) break;
            if (!active_levels.test(p)) continue;
            
            OrderNode* resting = book[p].head;
            while (agg->quantity > 0 && resting != nullptr) {
                uint64_t trade_qty = std::min(agg->quantity, resting->quantity);
                OrderNode* next_resting = resting->next;
                executeTrade(agg, resting, trade_qty);
                if (resting->quantity == 0) { book[p].remove(resting); deallocateNode(resting); }
                resting = next_resting;
            }
            if (book[p].empty()) active_levels.reset(p);
        }
    } else {
        for (int64_t p = MAX_PRICE_RANGE - 1; p >= 0 && p >= (int64_t)agg->price; --p) {
            if (agg->quantity == 0) break;
            if (!active_levels.test(p)) continue;
            
            OrderNode* resting = book[p].head;
            while (agg->quantity > 0 && resting != nullptr) {
                uint64_t trade_qty = std::min(agg->quantity, resting->quantity);
                OrderNode* next_resting = resting->next;
                executeTrade(agg, resting, trade_qty);
                if (resting->quantity == 0) { book[p].remove(resting); deallocateNode(resting); }
                resting = next_resting;
            }
            if (book[p].empty()) active_levels.reset(p);
        }
    }
}

#endif