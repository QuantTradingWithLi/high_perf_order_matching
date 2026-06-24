#ifndef FAST_IO_HPP
#define FAST_IO_HPP

#include <string_view>
#include <cstdint>
#include "MatchingEngine.hpp"

struct ParsedMessage {
    int error_code = 0; // 0 = success, -1 = format, -2 = type, -3 = numeric
    uint64_t id = 0;
    Side side = Side::BUY;
    uint64_t quantity = 0;
    uint64_t price = 0; 
};

class FastIO {
public:
    static ParsedMessage parseLine(std::string_view line);
    static void formatTrade(uint64_t qty, uint64_t price);
    static void formatFilled(uint64_t id);
    static void formatPartial(uint64_t id, uint64_t remaining_qty);
};

#endif // FAST_IO_HPP