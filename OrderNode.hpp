#ifndef ORDER_NODE_HPP
#define ORDER_NODE_HPP

#include <cstdint>

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Aligning to 32 bytes ensures that nodes fit efficiently within cache lines,
// preventing performance degradation due to crossing cache boundaries.
struct alignas(32) OrderNode {
    uint64_t id;
    uint64_t quantity;
    uint64_t price;
    Side side;
    OrderNode* next;
    OrderNode* prev;

    OrderNode() 
        : id(0), quantity(0), price(0), side(Side::BUY), next(nullptr), prev(nullptr) {}

    OrderNode(uint64_t id, uint64_t qty, uint64_t prc, Side s, OrderNode* n, OrderNode* p)
        : id(id), quantity(qty), price(prc), side(s), next(n), prev(p) {}
};

#endif // ORDER_NODE_HPP