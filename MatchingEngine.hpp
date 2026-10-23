#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include "OrderNode.hpp"

struct PriceLevel {
    OrderNode* head = nullptr;
    OrderNode* tail = nullptr;

    void push_back(OrderNode* node) noexcept {
        if (!head) { head = tail = node; node->next = node->prev = nullptr; }
        else { tail->next = node; node->prev = tail; node->next = nullptr; tail = node; }
    }
    void remove(OrderNode* node) noexcept {
        if (node->prev) node->prev->next = node->next; else head = node->next;
        if (node->next) node->next->prev = node->prev; else tail = node->prev;
    }
    bool empty() const noexcept { return head == nullptr; }
};

static constexpr size_t MAX_PRICE_RANGE = 20000000;

// Portable price bitmap. Unlike std::bitset's libstdc++-only _Find_first /
// _Find_next extensions, this supports searching in both directions and keeps
// the MatchingEngine object small (the words live on the heap).
class PriceBitmap {
public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    explicit PriceBitmap(size_t size)
        : words_((size + 63) / 64, 0), size_(size) {}

    void set(size_t index) noexcept {
        words_[index >> 6] |= (uint64_t{1} << (index & 63));
    }

    void reset(size_t index) noexcept {
        words_[index >> 6] &= ~(uint64_t{1} << (index & 63));
    }

    bool test(size_t index) const noexcept {
        return (words_[index >> 6] >> (index & 63)) & 1;
    }

    size_t find_first() const noexcept {
        return find_at_or_after(0);
    }

    // First set bit >= index.
    size_t find_at_or_after(size_t index) const noexcept {
        if (index >= size_) return npos;

        size_t word = index >> 6;
        uint64_t bits = words_[word] & (~uint64_t{0} << (index & 63));

        while (true) {
            if (bits) {
                const size_t found = (word << 6) + std::countr_zero(bits);
                return found < size_ ? found : npos;
            }
            if (++word >= words_.size()) return npos;
            bits = words_[word];
        }
    }

    // First set bit > index.
    size_t find_next(size_t index) const noexcept {
        if (index >= size_ || index == npos) return npos;
        return find_at_or_after(index + 1);
    }

    // First set bit < index.
    size_t find_prev(size_t index) const noexcept {
        if (index == 0 || words_.empty()) return npos;

        size_t last = index - 1;
        if (last >= size_) last = size_ - 1;

        size_t word = last >> 6;
        const size_t offset = last & 63;
        const uint64_t mask = offset == 63
            ? ~uint64_t{0}
            : ((uint64_t{1} << (offset + 1)) - 1);
        uint64_t bits = words_[word] & mask;

        while (true) {
            if (bits) {
                const size_t found = (word << 6) + (63 - std::countl_zero(bits));
                return found < size_ ? found : npos;
            }
            if (word == 0) return npos;
            bits = words_[--word];
        }
    }

private:
    std::vector<uint64_t> words_;
    size_t size_;
};

class MatchingEngine {
private:
    std::vector<OrderNode> orderPool;
    std::vector<OrderNode*> freeList;
    std::unordered_map<uint64_t, OrderNode*> directory;

    // A price level belongs exclusively to one side: a crossing order is
    // matched before it can rest, so bids and asks can never coexist at the
    // same price. One array halves the price-level memory.
    std::unique_ptr<PriceLevel[]> levels;
    PriceBitmap active_bids;
    PriceBitmap active_asks;

    size_t highest_bid = PriceBitmap::npos;
    size_t lowest_ask = PriceBitmap::npos;

    OrderNode* allocateNode() noexcept;
    void deallocateNode(OrderNode* node) noexcept;
    void executeTrade(OrderNode* agg, OrderNode* resting, uint64_t trade_qty) noexcept;
    void onLevelEmpty(Side resting_side, size_t price) noexcept;

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
    if constexpr (S == Side::BUY) {
        if (lowest_ask == PriceBitmap::npos || lowest_ask > agg->price) return;

        size_t p = lowest_ask;
        while (p != PriceBitmap::npos && p <= agg->price) {
            PriceLevel& level = levels[p];
            OrderNode* resting = level.head;

            while (agg->quantity > 0 && resting != nullptr) {
                const uint64_t trade_qty = std::min(agg->quantity, resting->quantity);
                OrderNode* next_resting = resting->next;

                executeTrade(agg, resting, trade_qty);
                if (resting->quantity == 0) {
                    level.remove(resting);
                    deallocateNode(resting);
                }
                resting = next_resting;
            }

            const size_t next = active_asks.find_next(p);
            if (level.empty()) onLevelEmpty(Side::SELL, p);
            if (agg->quantity == 0) break;
            p = next;
        }
    } else {
        if (highest_bid == PriceBitmap::npos || highest_bid < agg->price) return;

        size_t p = highest_bid;
        while (p != PriceBitmap::npos && p >= agg->price) {
            PriceLevel& level = levels[p];
            OrderNode* resting = level.head;

            while (agg->quantity > 0 && resting != nullptr) {
                const uint64_t trade_qty = std::min(agg->quantity, resting->quantity);
                OrderNode* next_resting = resting->next;

                executeTrade(agg, resting, trade_qty);
                if (resting->quantity == 0) {
                    level.remove(resting);
                    deallocateNode(resting);
                }
                resting = next_resting;
            }

            const size_t next = active_bids.find_prev(p);
            if (level.empty()) onLevelEmpty(Side::BUY, p);
            if (agg->quantity == 0) break;
            p = next;
        }
    }
}

#endif
