#ifndef FAST_IO_HPP
#define FAST_IO_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "MatchingEngine.hpp"

struct ParsedMessage {
    uint64_t id = 0;
    uint64_t quantity = 0;
    uint64_t price = 0;
    int error_code = 0; // 0 = success, -1 = format, -2 = type, -3 = numeric
    Side side = Side::BUY;
};

static_assert(std::is_trivially_copyable_v<ParsedMessage>);
static_assert(sizeof(ParsedMessage) == 32);

class FastIO {
public:
    using WriteFn = void (*)(const char* data, size_t size);

    static ParsedMessage parseLine(std::string_view line);
    static void formatTrade(uint64_t qty, uint64_t price);
    static void formatFilled(uint64_t id);
    static void formatPartial(uint64_t id, uint64_t remaining_qty);

    // Replace the output sink (nullptr restores the default stdout sink).
    static void setSink(WriteFn sink) noexcept;
    static void flush();
};

#endif // FAST_IO_HPP
