#ifndef ORDER_NODE_HPP
#define ORDER_NODE_HPP

#include <cstdint>

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Packed node stored in a contiguous pool. Links are 32-bit pool indices
// instead of pointers, which keeps the node at 32 bytes and avoids per-order
// heap allocation. The pool can hold at most OrderNode::NIL - 1 orders.
struct alignas(32) OrderNode {
    static constexpr uint32_t NIL = 0xFFFFFFFFu;

    uint64_t id = 0;
    uint64_t quantity = 0;
    uint32_t price = 0;
    uint32_t next = NIL;
    uint32_t prev = NIL;
    Side side = Side::BUY;

    OrderNode() = default;

    OrderNode(uint64_t id, uint64_t quantity, uint32_t price, Side side)
        : id(id), quantity(quantity), price(price), side(side) {}
};

static_assert(sizeof(OrderNode) == 32,
              "OrderNode must stay packed to 32 bytes");

#endif // ORDER_NODE_HPP
