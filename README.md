# High-Performance Order Matching Engine

## Disclaimer
_All materials in this repository, including text, code, and documentation, were created entirely by me. This project is an independent work inspired by common practices in quantitative finance and software design. It is **not affiliated with, endorsed by, or derived from any proprietary materials** belonging to any organization._


## Description
A high-performance order-matching engine implemented in C++20. It uses a pre-allocated order pool, packed index-linked order nodes, sparse price-level blocks, and a flat order directory to keep the steady-state matching path free of per-order heap allocation.

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

Prices are written with up to four decimal places, with trailing zeros removed. For example, an internal price of `1005000` is emitted as `100.5`, while `1000000` is emitted as `100`.

### Error Handling
The parser validates the complete input line, including trailing fields. It rejects unsupported sides, zero quantities, out-of-range prices, extra fields, and numeric fields with trailing junk. Invalid inputs are identified by the following codes:
* `-1:` Structural Error (e.g., malformed syntax, missing required fields, extra fields).
* `-2:` Unsupported Message Type (e.g., an ID provided for a type not recognized by the system).
* `-3:` Data Conversion or Range Error (e.g., non-numeric characters, invalid side, zero quantity, or price outside the configured range).

## Architectural Optimizations
* **Pre-Allocated, Sparse Book**: Order nodes, the order directory, and price bitmaps are sized at startup. Price levels are allocated in 64-price blocks only when a price is first touched, so an empty engine does not reserve a dense 20-million-level array.
* **Packed 32-Byte Nodes**: Order nodes use 32-bit pool indices for their intrusive links, keeping each node at 32 bytes. The order pool and free list are contiguous and reused after cancels.
* **Flat Order Directory**: An open-addressing, tombstone-based flat hash map replaces `std::unordered_map` for order-id lookup. The table is reserved at startup and grows only if necessary.
* **Bidirectional Price Bitmap**: A custom `PriceBitmap` provides best-price, next-price, and previous-price searches without `std::set` or non-portable `std::bitset` extensions.
* **Best-Price Tracking**: Exact best bid and ask indices avoid rescanning from price zero. Empty levels clear their bitmap bit and update the best price in the same operation.
* **CPU Affinity**: On Linux, the reader and matching threads attempt to pin to cores 0 and 1 with `pthread_setaffinity_np`; failure is reported as a warning and does not abort the run. Busy waits use the architecture pause instruction where available.
* **SPSC Lock-Free Input**: The reader parses each line directly into a fixed-size, trivially-copyable `ParsedMessage` and sends it through a cache-line-padded SPSC ring buffer. The engine thread does not parse text or copy `std::string` objects.
* **Buffered Output**: Trade and fill events are formatted with `std::to_chars` into a 64 KiB buffer rather than performing a stream insertion per field.
* **Noexcept Engine API**: Core add, cancel, and match paths are marked `noexcept`. The engine still allocates its pools and any newly touched price-level blocks; allocation failure in a `noexcept` path terminates the process rather than throwing.
* **Templated Matching Logic**: Template specialization for `Side::BUY` and `Side::SELL` eliminates side branching in the hot loop.

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
After building, run all CTest targets:

```bash
ctest --test-dir build --output-on-failure
```

The suites cover engine behavior, strict parsing and output formatting, randomized differential matching against a reference book, the price bitmap, the flat hash map, and the SPSC queue.

The original engine test binary can also be run directly:

```bash
./build/matching_engine_tests
```

## Benchmark
The benchmark reports structure sizes, engine construction time, and add-only, buy-sweep, sell-sweep, and cancel workloads with engine output suppressed:

```bash
./build/matching_engine_benchmark
```

## Deployment

Pushing a tag in the `vX.X.X` format runs the **Deploy** GitHub Actions workflow. It builds and tests the engine on Ubuntu 24.04, then publishes a GitHub Release containing:

* `matching-engine-vX.X.X-linux-x86_64.tar.gz` with the engine, benchmark, README, license, and sample input.
* A `.sha256` checksum for the archive.

```bash
git tag v0.0.1
git push origin v0.0.1
```

Release binaries are built with `-DENABLE_NATIVE_OPTIMIZATIONS=OFF` so they run on any x86-64 Linux CPU. Local builds still use `-march=native` by default; pass `-DENABLE_NATIVE_OPTIMIZATIONS=OFF` to `cmake` for a portable build.

## Performance Profile
* `Time Complexity`
	* `Add Order:` $O(1)$ expected. The flat directory performs a constant-time expected lookup/insert, the free list supplies a node without allocating, and the intrusive list append is constant time. A new 64-price level block is allocated only the first time that block is touched.
	* `Cancel Order:` $O(1)$ expected. The order directory provides direct access to the node. If the canceled order empties the best price level, the bitmap finds the next best set bit, which scans at most the remaining bitmap words.
	* `Matching/Traversal:` $O(K + T + W)$, where $K$ is the number of price levels touched, $T$ is the number of trades, and $W$ is the number of 64-bit bitmap words scanned while moving between active levels. The engine advances between active prices with bitmap-word scans instead of touching every tick slot.
	
* `Space Complexity:` $O(N + U + P/64)$, where $N$ is the maximum number of orders, $U$ is the number of touched 64-price level blocks, and $P$ is the price domain. Each order node is 32 bytes, each price level is 8 bytes, each sparse block is 512 bytes, and the two price bitmaps use $P/64$ bits each. The dense $P \times 16$-byte price-level array used by the previous implementation is gone; sparse blocks are allocated only on demand.