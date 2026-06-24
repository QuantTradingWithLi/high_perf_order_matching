#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP

#include <atomic>
#include <cstddef>
#include <array>

// Lock-free Single-Producer Single-Consumer Ring Buffer.
// Uses cache line padding to prevent "false sharing" where the 
// producer and consumer CPUs fight over the same cache line.
template <typename T, size_t Capacity>
class SPSCQueue {
private:
    alignas(64) std::atomic<size_t> tail{0};
    alignas(64) std::atomic<size_t> head{0};
    
    std::array<T, Capacity> buffer;

public:
    // Pushes an item to the queue. Returns false if full.
    bool push(const T& item) {
        const size_t current_tail = tail.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % Capacity;
        
        // Ensure we don't overwrite the consumer's position
        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; 
        }
        
        buffer[current_tail] = item;
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    // Pops an item from the queue. Returns false if empty.
    bool pop(T& item) {
        const size_t current_head = head.load(std::memory_order_relaxed);
        
        if (current_head == tail.load(std::memory_order_acquire)) {
            return false;
        }
        
        item = buffer[current_head];
        head.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }
};

#endif // SPSC_QUEUE_HPP