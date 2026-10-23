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
* **Order Book Entry**: Any remaining quantity of an aggressive order that cannot be matched is added to the book as a new resting order when a pool slot is available.
* **Capacity**: The executable pre-allocates 1,000,000 order slots. If the pool is exhausted, an unmatched residual is not added to the book.



### 2. Message Protocol
The system processes comma-separated input strings.

**Input Message Types:**
* **Add Order (Type 0)**: Places a new limit order into the book.
    * *Format:* `0,ID,Side,Quantity,Price`
    * *Side:* 0 for Buy, 1 for Sell.
* **Cancel Order (Type 1)**: Removes an existing order from the book.
    * *Format:* `1,ID`

The executable is line-oriented and ignores blank lines and lines beginning with `#`. IDs and quantities are unsigned 64-bit integers; quantity must be greater than zero. Prices are fixed-point values with up to four decimal places and are accepted from `0` through `1999.9999`; an integer price is interpreted as whole units. For example, `100.5` is stored internally as `1005000`. Fields are not whitespace-trimmed; a single trailing `\r` is accepted for CRLF input.

**Output Event Notifications:**
The system emits newline-delimited events to `stdout` to notify the system of market activity:
* **Trade Execution**: `2,<quantity>,<price>`
* **Fill Status**: `3,<order_id>`
* **Partial Fill**: `3,<order_id>,<remaining_quantity>`

Each match emits a trade event followed by status events for the aggressive and resting orders. Prices are written with up to four decimal places, with trailing zeros removed. For example, an internal price of `1005000` is emitted as `100.5`, while `1000000` is emitted as `100`.

### Error Handling
The parser validates the complete input line, including trailing fields. It rejects unsupported sides, zero quantities, out-of-range prices, extra fields, and numeric fields with trailing junk. Invalid lines are reported to `stderr` as `Error <code> processing input: <line>` and processing continues. Codes:
* `-1:` Structural Error (e.g., malformed syntax, missing required fields, extra fields).
* `-2:` Unsupported Message Type (a type other than 0 or 1).
* `-3:` Data Conversion or Range Error (e.g., non-numeric characters, invalid side, zero quantity, trailing junk, or a price outside `0` through `1999.9999`).

An Add with an already active ID is ignored, and a Cancel for an inactive ID is ignored.

## Architectural Optimizations
* **Pre-Allocated, Sparse Book**: Order nodes, the order directory, and price bitmaps are sized at startup. Price levels are allocated in 64-price blocks when a block is first touched, so an empty engine does not reserve a dense 20-million-level array.
* **Packed 32-Byte Nodes**: Order nodes use 32-bit pool indices for their intrusive links, keeping each node at 32 bytes. The order pool and free list are contiguous and reused after cancels and fills.
* **Flat Order Directory**: An open-addressing, tombstone-based flat hash map replaces `std::unordered_map` for order-id lookup. The table is reserved at startup and rehashes only when load or tombstones require it.
* **Bidirectional Price Bitmap**: A custom `PriceBitmap` provides best-price, next-price, and previous-price searches without `std::set` or non-portable `std::bitset` extensions.
* **Best-Price Tracking**: Exact best bid and ask indices avoid rescanning from price zero. Empty levels clear their bitmap bit; if the emptied level was the best, the best price is updated in the same operation.
* **CPU Affinity**: On Linux, the reader and matching threads attempt to pin to cores 0 and 1 with `pthread_setaffinity_np`; failure is reported as a warning and does not abort the run. Busy waits use the architecture pause instruction where available.
* **SPSC Lock-Free Input**: The reader parses each line directly into a fixed-size, trivially-copyable `ParsedMessage` and sends it through a cache-line-padded SPSC ring buffer. The engine thread does not parse text or copy `std::string` objects.
* **Buffered Output**: Trade and fill events are formatted into a 64 KiB buffer rather than performing a stream insertion per field; integer fields use `std::to_chars`, while prices use fixed-point formatting.
* **Noexcept Engine API**: Core add, cancel, and match paths are marked `noexcept`. The engine still allocates its pools, newly touched price-level blocks, and occasional directory rehashes; allocation failure in a `noexcept` path terminates the process rather than throwing.
* **Templated Matching Logic**: Compile-time dispatch through `processOrder<Side::BUY>` and `processOrder<Side::SELL>` keeps side checks out of the match loop.

## Build Instructions (Ubuntu 24.04 LTS)
Ensure CMake and a modern C++20 compiler (`g++` or `clang++`) are installed. From the repository root:

```bash
cmake -S . -B build
cmake --build build --parallel
```

Local builds use `-march=native` by default. Add `-DENABLE_NATIVE_OPTIMIZATIONS=OFF` to the configure command for a portable x86-64 build.

## Execution
From the repository root, run the engine. It attempts to pin its reader and engine threads to isolated cores (if available):

```bash
./build/matching_engine < test_data.txt
```

## Testing
After building, run all CTest targets from the repository root:

```bash
ctest --test-dir build --output-on-failure
```

The suites cover engine behavior, strict parsing and output formatting, randomized differential matching against a reference book, the price bitmap, the flat hash map, and the SPSC queue.

The primary engine test binary can also be run directly:

```bash
./build/matching_engine_tests
```

## Benchmark
The benchmark reports structure sizes, engine construction time, and add-only, buy-sweep, sell-sweep, and cancel workloads with engine output suppressed:

```bash
./build/matching_engine_benchmark
```

## Deployment

Pushing a tag matching `vMAJOR.MINOR.PATCH` runs the **Deploy** GitHub Actions workflow. It builds all targets, runs the primary unit tests and a smoke test on Ubuntu 24.04, then publishes a GitHub Release containing:

* `matching-engine-vX.Y.Z-linux-x86_64.tar.gz` with the engine, benchmark, README, license, and sample input.
* A `.sha256` checksum for the archive.

```bash
git tag -a vX.Y.Z -m "vX.Y.Z"
git push origin vX.Y.Z
```

Release binaries are built with `-DENABLE_NATIVE_OPTIMIZATIONS=OFF`, so they target the x86-64 baseline rather than the build machine's native ISA. The archive is built on Ubuntu 24.04, so target systems need a compatible `glibc` and `libstdc++`.

## Performance Profile
* `Time Complexity`
	* `Add Order:` Expected amortized $O(1)$. The flat directory performs a constant-time expected lookup/insert, the free list supplies a node without allocating, and the intrusive list append is constant time. A new 64-price level block is allocated only the first time that block is touched.
	* `Cancel Order:` Expected $O(1)$ through the order directory. If the canceled order empties the best price level, the bitmap finds the next best set bit in $O(W)$, where $W$ is the number of 64-bit words scanned.
	* `Matching/Traversal:` $O(K + T + W)$, where $K$ is the number of price levels touched, $T$ is the number of trades, and $W$ is the number of 64-bit bitmap words scanned while moving between active levels. The engine advances between active prices with bitmap-word scans instead of touching every tick slot.
	
* `Space Complexity:` $O(N + U + P/64)$, where $N$ is the maximum number of orders, $U$ is the number of touched 64-price level blocks, and $P$ is the price domain. Each order node is 32 bytes, each price level is 8 bytes, each sparse block is 512 bytes, and the two price bitmaps use $P/64$ bits each. The dense $P \times 16$-byte price-level array used by the previous implementation is gone; sparse blocks are allocated only on demand.