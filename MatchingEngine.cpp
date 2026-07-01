#include "MatchingEngine.hpp"
#include "FastIO.hpp"

MatchingEngine::MatchingEngine(size_t max_orders) noexcept 
    : bids(std::make_unique<PriceLevel[]>(MAX_PRICE_RANGE)),
      asks(std::make_unique<PriceLevel[]>(MAX_PRICE_RANGE)),
      active_bids(),
      active_asks(),
      highest_bid(0),
      lowest_ask(MAX_PRICE_RANGE) {
    orderPool.resize(max_orders);
    freeList.reserve(max_orders);
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

void MatchingEngine::addOrder(uint64_t id, Side side, uint64_t quantity, uint64_t price) noexcept {
    if (price >= MAX_PRICE_RANGE) return;
    OrderNode agg_temp{id, quantity, price, side, nullptr, nullptr};

    if (side == Side::BUY) {
        processOrder<Side::BUY>(&agg_temp);
        if (agg_temp.quantity > 0) {
            OrderNode* node = allocateNode();
            if (node) {
                *node = agg_temp;
                bids[price].push_back(node);
                active_bids.set(price); // Use .set()
                if (price > highest_bid) highest_bid = price;
                directory[id] = node;
            }
        }
    } else {
        processOrder<Side::SELL>(&agg_temp);
        if (agg_temp.quantity > 0) {
            OrderNode* node = allocateNode();
            if (node) {
                *node = agg_temp;
                asks[price].push_back(node);
                active_asks.set(price); // Use .set()
                if (price < lowest_ask) lowest_ask = price;
                directory[id] = node;
            }
        }
    }
}

void MatchingEngine::cancelOrder(uint64_t id) noexcept {
    auto it = directory.find(id);
    if (it != directory.end()) {
        OrderNode* node = it->second;
        if (node->price >= MAX_PRICE_RANGE) return;
        if (node->side == Side::BUY) {
            bids[node->price].remove(node);
            if (bids[node->price].empty()) active_bids.reset(node->price); // Use .reset()
        } else {
            asks[node->price].remove(node);
            if (asks[node->price].empty()) active_asks.reset(node->price); // Use .reset()
        }
        deallocateNode(node);
    }
}

bool MatchingEngine::isOrderActive(uint64_t id) const noexcept {
    return directory.find(id) != directory.end();
}