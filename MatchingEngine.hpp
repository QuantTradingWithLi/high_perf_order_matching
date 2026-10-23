#ifndef MATCHING_ENGINE_HPP
#define MATCHING_ENGINE_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "FlatHashMap.hpp"
#include "OrderNode.hpp"

struct PriceLevel {
    uint32_t head = OrderNode::NIL;
    uint32_t tail = OrderNode::NIL;

    void push_back(OrderNode* nodes, uint32_t index) noexcept {
        OrderNode& node = nodes[index];
        node.prev = tail;
        node.next = OrderNode::NIL;

        if (tail != OrderNode::NIL) nodes[tail].next = index;
        else head = index;
        tail = index;
    }

    void remove(OrderNode* nodes, uint32_t index) noexcept {
        const OrderNode& node = nodes[index];
        if (node.prev != OrderNode::NIL) nodes[node.prev].next = node.next;
        else head = node.next;

        if (node.next != OrderNode::NIL) nodes[node.next].prev = node.prev;
        else tail = node.prev;
    }

    bool empty() const noexcept { return head == OrderNode::NIL; }
};

static_assert(sizeof(PriceLevel) == 8, "PriceLevel must stay packed to 8 bytes");

static constexpr uint32_t MAX_PRICE_RANGE = 20000000;

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
    static constexpr size_t LEVEL_BLOCK_SHIFT = 6;
    static constexpr size_t LEVEL_BLOCK_SIZE = size_t{1} << LEVEL_BLOCK_SHIFT;
    static constexpr size_t LEVEL_BLOCK_MASK = LEVEL_BLOCK_SIZE - 1;
    static constexpr size_t LEVEL_BLOCK_COUNT =
        (static_cast<size_t>(MAX_PRICE_RANGE) + LEVEL_BLOCK_SIZE - 1) /
        LEVEL_BLOCK_SIZE;
    static constexpr uint32_t NO_PRICE = OrderNode::NIL;

    std::vector<OrderNode> orderPool;
    std::vector<uint32_t> freeList;
    FlatHashMap directory;

    // Price levels are allocated in small blocks on first use. The bitmap
    // tracks which prices are active, so matching never scans the full domain.
    std::vector<std::unique_ptr<PriceLevel[]>> levelBlocks;
    PriceBitmap active_bids;
    PriceBitmap active_asks;

    uint32_t highest_bid = NO_PRICE;
    uint32_t lowest_ask = NO_PRICE;

    static size_t clampCapacity(size_t capacity) noexcept {
        return capacity >= OrderNode::NIL
            ? static_cast<size_t>(OrderNode::NIL) - 1
            : capacity;
    }

    static uint32_t toPrice(size_t index) noexcept {
        return index == PriceBitmap::npos
            ? NO_PRICE
            : static_cast<uint32_t>(index);
    }

    PriceLevel& levelAt(uint32_t price) noexcept {
        return levelBlocks[price >> LEVEL_BLOCK_SHIFT][price & LEVEL_BLOCK_MASK];
    }

    PriceLevel& ensureLevelAt(uint32_t price) {
        auto& block = levelBlocks[price >> LEVEL_BLOCK_SHIFT];
        if (!block) {
            block = std::make_unique<PriceLevel[]>(LEVEL_BLOCK_SIZE);
        }
        return block[price & LEVEL_BLOCK_MASK];
    }

    uint32_t allocateNode() noexcept;
    void deallocateNode(uint32_t index) noexcept;
    void executeTrade(OrderNode& agg, OrderNode& resting,
                      uint64_t trade_qty) noexcept;
    void onLevelEmpty(Side resting_side, uint32_t price) noexcept;

    template <Side S>
    void processOrder(OrderNode* agg) noexcept;

public:
    explicit MatchingEngine(size_t max_orders = 1000000) noexcept;
    void addOrder(uint64_t id, Side side, uint64_t quantity,
                  uint64_t price) noexcept;
    void cancelOrder(uint64_t id) noexcept;
    bool isOrderActive(uint64_t id) const noexcept;
};

template <Side S>
void MatchingEngine::processOrder(OrderNode* agg) noexcept {
    if constexpr (S == Side::BUY) {
        if (lowest_ask == NO_PRICE || lowest_ask > agg->price) return;

        uint32_t p = lowest_ask;
        while (p != NO_PRICE && p <= agg->price) {
            PriceLevel& level = levelAt(p);
            uint32_t resting_index = level.head;

            while (agg->quantity > 0 && resting_index != OrderNode::NIL) {
                OrderNode& resting = orderPool[resting_index];
                const uint32_t next_resting = resting.next;
                const uint64_t trade_qty =
                    std::min(agg->quantity, resting.quantity);

                executeTrade(*agg, resting, trade_qty);
                if (resting.quantity == 0) {
                    level.remove(orderPool.data(), resting_index);
                    deallocateNode(resting_index);
                }
                resting_index = next_resting;
            }

            const uint32_t next = toPrice(active_asks.find_next(p));
            if (level.empty()) onLevelEmpty(Side::SELL, p);
            if (agg->quantity == 0) break;
            p = next;
        }
    } else {
        if (highest_bid == NO_PRICE || highest_bid < agg->price) return;

        uint32_t p = highest_bid;
        while (p != NO_PRICE && p >= agg->price) {
            PriceLevel& level = levelAt(p);
            uint32_t resting_index = level.head;

            while (agg->quantity > 0 && resting_index != OrderNode::NIL) {
                OrderNode& resting = orderPool[resting_index];
                const uint32_t next_resting = resting.next;
                const uint64_t trade_qty =
                    std::min(agg->quantity, resting.quantity);

                executeTrade(*agg, resting, trade_qty);
                if (resting.quantity == 0) {
                    level.remove(orderPool.data(), resting_index);
                    deallocateNode(resting_index);
                }
                resting_index = next_resting;
            }

            const uint32_t next = toPrice(active_bids.find_prev(p));
            if (level.empty()) onLevelEmpty(Side::BUY, p);
            if (agg->quantity == 0) break;
            p = next;
        }
    }
}

#endif // MATCHING_ENGINE_HPP
