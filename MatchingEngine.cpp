#include "MatchingEngine.hpp"
#include "FastIO.hpp"

MatchingEngine::MatchingEngine(size_t max_orders) noexcept
    : levels(std::make_unique<PriceLevel[]>(MAX_PRICE_RANGE)),
      active_bids(MAX_PRICE_RANGE),
      active_asks(MAX_PRICE_RANGE) {
    orderPool.resize(max_orders);
    freeList.reserve(max_orders);
    directory.reserve(max_orders);
    for (size_t i = 0; i < max_orders; ++i) freeList.push_back(&orderPool[i]);
}

OrderNode* MatchingEngine::allocateNode() noexcept {
    if (freeList.empty()) return nullptr;
    OrderNode* node = freeList.back();
    freeList.pop_back();
    return node;
}

void MatchingEngine::deallocateNode(OrderNode* node) noexcept {
    directory.erase(node->id);
    freeList.push_back(node);
}

void MatchingEngine::executeTrade(OrderNode* agg, OrderNode* resting, uint64_t trade_qty) noexcept {
    agg->quantity -= trade_qty;
    resting->quantity -= trade_qty;
    FastIO::formatTrade(trade_qty, resting->price);
    if (agg->quantity == 0) FastIO::formatFilled(agg->id);
    else FastIO::formatPartial(agg->id, agg->quantity);

    if (resting->quantity == 0) FastIO::formatFilled(resting->id);
    else FastIO::formatPartial(resting->id, resting->quantity);
}

void MatchingEngine::onLevelEmpty(Side resting_side, size_t price) noexcept {
    if (resting_side == Side::BUY) {
        active_bids.reset(price);
        if (price == highest_bid) highest_bid = active_bids.find_prev(price);
    } else {
        active_asks.reset(price);
        if (price == lowest_ask) lowest_ask = active_asks.find_next(price);
    }
}

void MatchingEngine::addOrder(uint64_t id, Side side, uint64_t quantity, uint64_t price) noexcept {
    if (price >= MAX_PRICE_RANGE) return;
    if (side != Side::BUY && side != Side::SELL) return;
    if (directory.find(id) != directory.end()) return;

    OrderNode agg_temp{id, quantity, price, side, nullptr, nullptr};

    if (side == Side::BUY) {
        processOrder<Side::BUY>(&agg_temp);
        if (agg_temp.quantity > 0) {
            OrderNode* node = allocateNode();
            if (!node) return;
            *node = agg_temp;
            levels[price].push_back(node);
            active_bids.set(price);
            if (highest_bid == PriceBitmap::npos || price > highest_bid) highest_bid = price;
            directory.emplace(id, node);
        }
    } else {
        processOrder<Side::SELL>(&agg_temp);
        if (agg_temp.quantity > 0) {
            OrderNode* node = allocateNode();
            if (!node) return;
            *node = agg_temp;
            levels[price].push_back(node);
            active_asks.set(price);
            if (lowest_ask == PriceBitmap::npos || price < lowest_ask) lowest_ask = price;
            directory.emplace(id, node);
        }
    }
}

void MatchingEngine::cancelOrder(uint64_t id) noexcept {
    auto it = directory.find(id);
    if (it == directory.end()) return;

    OrderNode* node = it->second;
    PriceLevel& level = levels[node->price];
    level.remove(node);
    if (level.empty()) onLevelEmpty(node->side, node->price);
    deallocateNode(node);
}

bool MatchingEngine::isOrderActive(uint64_t id) const noexcept {
    return directory.find(id) != directory.end();
}
