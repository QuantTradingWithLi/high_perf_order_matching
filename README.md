# High-Performance Order Matching Engine

## Disclaimer
_All materials in this repository, including text, code, and documentation, were created entirely by me. This project is an independent work inspired by common practices in quantitative finance and software design. It is **not affiliated with, endorsed by, or derived from any proprietary materials** belonging to any organization._


## Description
A high-performance order-matching engine implemented in C++20. This engine minimizes jitter and latency through zero-allocation object pools, intrusive linked lists, and HFT-standard performance primitives.

## Functional Specification

The matching engine acts as a continuous double-sided auction system. It processes incoming order messages, maintains a state of the market, and emits execution events based on a Price-Time priority model.

### 1. Market Mechanics
The engine operates a Limit Order Book (LOB) which serves two sides of the market:
* **Bids (Buy Orders)**: Interested in buying at the highest possible price.
* **Asks (Sell Orders)**: Interested in selling at the lowest possible price.

**Matching Rules:**
* **Price-Time Priority**: Resting orders are prioritized by their price level (best price first). If multiple orders exist at the same price, the order that arrived earliest (FIFO) is matched first.
* **Crossing Orders**: An incoming "Aggressive" order is matched against the best available "Resting" orders on the opposite side of the book. 
* **Execution**: Trades execute at the resting order's price. If an aggressive order has a larger quantity than the resting order, it consumes the resting order entirely and continues to the next best price level until either the aggressive order is fully filled or it runs out of liquidity.
* **Order Book Entry**: Any remaining quantity of an aggressive order that cannot be matched is added to the book as a new resting order.



### 2. Message Protocol
The system processes comma-separated input strings.

**Input Message Types:**
* **Add Order (Type 0)**: Places a new limit order into the book.
    * *Format:* `0,ID,Side,Quantity,Price`
    * *Side:* 0 for Buy, 1 for Sell.
* **Cancel Order (Type 1)**: Removes an existing order from the book.
    * *Format:* `1,ID`

**Output Event Notifications:**
The system emits events to `stdout` to notify the system of market activity:
* **Trade Execution**: Emitted when a match occurs (Price and Quantity).
* **Fill Status**: Emitted when an order is fully satisfied (Order ID).
* **Partial Fill**: Emitted when an order is partially satisfied, reporting the remaining quantity (Order ID, Remaining Qty).

### Error Handling
The parser implements strict validation. Invalid inputs are identified by the following codes:
* `-1:` Structural Error (e.g., malformed syntax, missing required fields).
* `-2:` Unsupported Message Type (e.g., an ID provided for a type not recognized by the system).
* `-3:` Data Conversion Error (e.g., non-numeric characters in numeric fields).

## Architectural Optimizations
* **Zero-Allocation Pipeline**: Utilizes pre-allocated object pools and intrusive linked lists to eliminate `malloc`/`free` during runtime.
* **CPU Affinity**: The matching engine is pinned to dedicated CPU cores via `pthread_setaffinity_np` to prevent kernel-level context switching and cache invalidation.
* **Bitset-Indexed Price Tracking**: Replaces `std::set` (Red-Black Tree) with a custom `std::bitset` based lookup, ensuring $O(1)$ price level management and cache-friendly traversal.
* **Branch Prediction**: Critical paths are decorated with `[[likely]]` and `[[unlikely]]` attributes.
* **Noexcept Pipeline**: Core logic is marked `noexcept`, allowing the compiler to optimize out stack unwinding tables.
* **SPSC Lock-Free I/O**: Asynchronous I/O handling via a lock-free Single-Producer Single-Consumer ring buffer.
* **Templated Matching Logic**: Template specialization for `Side::BUY` and `Side::SELL` eliminates branching in the hot path.

## Build Instructions (Ubuntu 24.04 LTS)
Ensure CMake and a modern C++20 compiler (`g++` or `clang++`) are installed.

```bash
mkdir build && cd build
cmake ..
make
```

## Execution
Run the engine, pinning it to isolated cores (if available):

```bash
./matching_engine < ../test_data.txt
```

## Testing
To run the internal unit tests (which validate the memory pool, intrusive lists, and strict error code parsing):

```bash
./matching_engine_tests
```

## Performance Profile
* `Time Complexity`
	* Add Order: $O(1)$ average case. The insertion into the std::unordered_map is $O(1)$ average, and finding the correct PriceLevel via the std::bitset is effectively $O(1)$ due to hardware-accelerated bit manipulation (scanning 64-bit words at a time). The subsequent insertion into the intrusive linked list at that price level is $O(1)$.
	* Cancel Order: $O(1)$ average case. By using an std::unordered_map to store pointers to OrderNode objects, you achieve constant-time lookup and removal from the intrusive linked list.
	* Matching/Traversal: $O(K)$, where $K$ is the number of active price levels containing liquidity. Because the std::bitset allows you to skip all empty price levels, you avoid the $O(P)$ complexity (where $P$ is the total tick domain of 20,000,000).
	
* `Space Complexity:` $O(N + P)$, where $N$ is the maximum number of orders (governed by your freeList capacity) and $P$ is the total price domain (the size of your std::bitset and price-level arrays). Memory usage is pre-allocated at start-up to ensure deterministic performance.
