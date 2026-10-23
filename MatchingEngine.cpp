#include "MatchingEngine.hpp"

#include "FastIO.hpp"

MatchingEngine::MatchingEngine(size_t max_orders) noexcept
    : directory(clampCapacity(max_orders)),
      levelBlocks(LEVEL_BLOCK_COUNT),
      active_bids(MAX_PRICE_RANGE),
      active_asks(MAX_PRICE_RANGE) {
    max_orders = clampCapacity(max_orders);
    orderPool.resize(max_orders);
    freeList.reserve(max_orders);
    for (size_t i = 0; i < max_orders; ++i) {
        freeList.push_back(static_cast<uint32_t>(i));
    }
}

uint32_t MatchingEngine::allocateNode() noexcept {
    if (freeList.empty()) return OrderNode::NIL;

    const uint32_t index = freeList.back();
    freeList.pop_back();
    return index;
}

void MatchingEngine::deallocateNode(uint32_t index) noexcept {
    OrderNode& node = orderPool[index];
    directory.erase(node.id);
    node.next = OrderNode::NIL;
    node.prev = OrderNode::NIL;
    freeList.push_back(index);
}

void MatchingEngine::executeTrade(OrderNode& agg, OrderNode& resting,
                                  uint64_t trade_qty) noexcept {
    agg.quantity -= trade_qty;
    resting.quantity -= trade_qty;
    FastIO::formatTrade(trade_qty, resting.price);
    if (agg.quantity == 0) FastIO::formatFilled(agg.id);
    else FastIO::formatPartial(agg.id, agg.quantity);

    if (resting.quantity == 0) FastIO::formatFilled(resting.id);
    else FastIO::formatPartial(resting.id, resting.quantity);
}

void MatchingEngine::onLevelEmpty(Side resting_side, uint32_t price) noexcept {
    if (resting_side == Side::BUY) {
        active_bids.reset(price);
        if (price == highest_bid) {
            highest_bid = toPrice(active_bids.find_prev(price));
        }
    } else {
        active_asks.reset(price);
        if (price == lowest_ask) {
            lowest_ask = toPrice(active_asks.find_next(price));
        }
    }
}

void MatchingEngine::addOrder(uint64_t id, Side side, uint64_t quantity,
                              uint64_t price) noexcept {
    if (quantity == 0 || price >= MAX_PRICE_RANGE) return;
    if (side != Side::BUY && side != Side::SELL) return;
    if (directory.find(id) != nullptr) return;

    const uint32_t price32 = static_cast<uint32_t>(price);
    OrderNode agg_temp{id, quantity, price32, side};

    auto restRemainder = [&] {
        const uint32_t index = allocateNode();
        if (index == OrderNode::NIL) return;

        orderPool[index] = agg_temp;
        ensureLevelAt(price32).push_back(orderPool.data(), index);

        if (side == Side::BUY) {
            active_bids.set(price32);
            if (highest_bid == NO_PRICE || price32 > highest_bid) {
                highest_bid = price32;
            }
        } else {
            active_asks.set(price32);
            if (lowest_ask == NO_PRICE || price32 < lowest_ask) {
                lowest_ask = price32;
            }
        }

        directory.insert(id, index);
    };

    if (side == Side::BUY) {
        processOrder<Side::BUY>(&agg_temp);
        if (agg_temp.quantity > 0) restRemainder();
    } else {
        processOrder<Side::SELL>(&agg_temp);
        if (agg_temp.quantity > 0) restRemainder();
    }
}

void MatchingEngine::cancelOrder(uint64_t id) noexcept {
    uint32_t* index_ptr = directory.find(id);
    if (!index_ptr) return;

    const uint32_t index = *index_ptr;
    OrderNode& node = orderPool[index];
    PriceLevel& level = levelAt(node.price);
    level.remove(orderPool.data(), index);
    if (level.empty()) onLevelEmpty(node.side, node.price);
    deallocateNode(index);
}

bool MatchingEngine::isOrderActive(uint64_t id) const noexcept {
    return directory.find(id) != nullptr;
}
