#ifndef FLAT_HASH_MAP_HPP
#define FLAT_HASH_MAP_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

// Fixed-capacity open-addressing hash map for the order-id directory.
// Keys are order ids; values are node-pool indices. The table is reserved at
// engine construction, so steady-state insert/find/erase do not allocate.
class FlatHashMap {
public:
    static constexpr uint32_t NIL = 0xFFFFFFFFu;

    explicit FlatHashMap(size_t expected_size = 0) {
        reserve(expected_size);
    }

    void reserve(size_t expected_size) {
        size_t capacity = 1;
        const size_t target = expected_size > 0 ? expected_size * 2 : 1;
        while (capacity < target) capacity <<= 1;

        slots_.assign(capacity, Slot{});
        mask_ = capacity - 1;
        size_ = 0;
        tombstones_ = 0;
    }

    uint32_t* find(uint64_t key) noexcept {
        return const_cast<uint32_t*>(static_cast<const FlatHashMap*>(this)->find(key));
    }

    const uint32_t* find(uint64_t key) const noexcept {
        if (slots_.empty()) return nullptr;

        size_t index = hash(key) & mask_;
        while (true) {
            const Slot& slot = slots_[index];
            if (slot.state == EMPTY) return nullptr;
            if (slot.state == OCCUPIED && slot.key == key) return &slot.value;
            index = (index + 1) & mask_;
        }
    }

    bool insert(uint64_t key, uint32_t value) noexcept {
        if (value == NIL || slots_.empty()) return false;

        if ((size_ + tombstones_ + 1) * 10 >= slots_.size() * 7) {
            const size_t new_capacity =
                (size_ + 1) * 2 >= slots_.size() ? slots_.size() * 2 : slots_.size();
            rehash(new_capacity);
        }

        size_t index = hash(key) & mask_;
        size_t first_tombstone = npos;
        while (true) {
            Slot& slot = slots_[index];
            if (slot.state == EMPTY) {
                const size_t target = first_tombstone == npos ? index : first_tombstone;
                Slot& inserted = slots_[target];
                inserted.key = key;
                inserted.value = value;
                inserted.state = OCCUPIED;
                ++size_;
                if (first_tombstone != npos) --tombstones_;
                return true;
            }
            if (slot.state == OCCUPIED) {
                if (slot.key == key) return false;
            } else if (first_tombstone == npos) {
                first_tombstone = index;
            }
            index = (index + 1) & mask_;
        }
    }

    bool erase(uint64_t key) noexcept {
        if (slots_.empty()) return false;

        size_t index = hash(key) & mask_;
        while (true) {
            Slot& slot = slots_[index];
            if (slot.state == EMPTY) return false;
            if (slot.state == OCCUPIED && slot.key == key) {
                slot.state = TOMBSTONE;
                slot.value = NIL;
                --size_;
                ++tombstones_;
                return true;
            }
            index = (index + 1) & mask_;
        }
    }

    size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

private:
    static constexpr size_t npos = static_cast<size_t>(-1);

    enum State : uint8_t {
        EMPTY = 0,
        OCCUPIED = 1,
        TOMBSTONE = 2
    };

    struct Slot {
        uint64_t key = 0;
        uint32_t value = NIL;
        State state = EMPTY;
    };

    static_assert(sizeof(Slot) == 16);

    static uint64_t hash(uint64_t x) noexcept {
        // splitmix64 finalizer: cheap and well distributed for integer ids.
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    void insert_no_grow(uint64_t key, uint32_t value) noexcept {
        size_t index = hash(key) & mask_;
        while (slots_[index].state == OCCUPIED) {
            index = (index + 1) & mask_;
        }
        Slot& slot = slots_[index];
        slot.key = key;
        slot.value = value;
        slot.state = OCCUPIED;
        ++size_;
    }

    void rehash(size_t new_capacity) {
        std::vector<Slot> old = std::move(slots_);
        slots_.assign(new_capacity, Slot{});
        mask_ = new_capacity - 1;
        size_ = 0;
        tombstones_ = 0;

        for (const Slot& slot : old) {
            if (slot.state == OCCUPIED) insert_no_grow(slot.key, slot.value);
        }
    }

    std::vector<Slot> slots_;
    size_t mask_ = 0;
    size_t size_ = 0;
    size_t tombstones_ = 0;
};

#endif // FLAT_HASH_MAP_HPP
