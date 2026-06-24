#include "FastIO.hpp"
#include <iostream>
#include <charconv>
#include <array>

constexpr uint64_t PRICE_MULTIPLIER = 10000;
static constexpr std::array<uint64_t, 5> POWERS_OF_10 = {10000, 1000, 100, 10, 1};

ParsedMessage FastIO::parseLine(std::string_view line) {
    ParsedMessage msg;
    const char* ptr = line.data();
    const char* end = line.data() + line.size();

    auto next_token = [&]() -> std::string_view {
        const char* start = ptr;
        while (ptr < end && *ptr != ',') ++ptr;
        std::string_view token(start, ptr - start);
        if (ptr < end) ++ptr;
        return token;
    };

    std::string_view type_str = next_token();
    if (type_str.empty()) [[unlikely]] { msg.error_code = -1; return msg; }

    int type = 0;
    if (std::from_chars(type_str.data(), type_str.data() + type_str.size(), type).ec != std::errc()) [[unlikely]] { msg.error_code = -3; return msg; }

    if (type == 1) { // Cancel: Type, ID
        std::string_view id_str = next_token();
        if (id_str.empty()) { msg.error_code = -1; return msg; }
        if (std::from_chars(id_str.data(), id_str.data() + id_str.size(), msg.id).ec != std::errc()) { msg.error_code = -3; return msg; }
        msg.error_code = 1;
    } else if (type == 0) { // Add: Type, ID, Side, Qty, Price
        // ID
        std::string_view id_str = next_token();
        if (id_str.empty()) { msg.error_code = -1; return msg; }
        if (std::from_chars(id_str.data(), id_str.data() + id_str.size(), msg.id).ec != std::errc()) { msg.error_code = -3; return msg; }
        
        // Side
        std::string_view side_str = next_token();
        int s = 0;
        if (side_str.empty() || std::from_chars(side_str.data(), side_str.data() + side_str.size(), s).ec != std::errc()) { msg.error_code = -3; return msg; }
        msg.side = static_cast<Side>(s);

        // Quantity
        std::string_view qty_str = next_token();
        if (qty_str.empty() || std::from_chars(qty_str.data(), qty_str.data() + qty_str.size(), msg.quantity).ec != std::errc()) { msg.error_code = -3; return msg; }

        // Price
        std::string_view price_str = next_token();
        if (price_str.empty()) { msg.error_code = -1; return msg; }
        
        size_t dot_pos = price_str.find('.');
        uint64_t integer_part = 0, fractional_part = 0;
        
        if (dot_pos == std::string_view::npos) {
            if (std::from_chars(price_str.data(), price_str.data() + price_str.size(), integer_part).ec != std::errc()) { msg.error_code = -3; return msg; }
            msg.price = integer_part * PRICE_MULTIPLIER;
        } else {
            if (std::from_chars(price_str.data(), price_str.data() + dot_pos, integer_part).ec != std::errc()) { msg.error_code = -3; return msg; }
            std::string_view frac_str = price_str.substr(dot_pos + 1);
            if (std::from_chars(frac_str.data(), frac_str.data() + frac_str.size(), fractional_part).ec != std::errc()) { msg.error_code = -3; return msg; }
            
            if (frac_str.size() < POWERS_OF_10.size()) {
                msg.price = (integer_part * PRICE_MULTIPLIER) + (fractional_part * POWERS_OF_10[frac_str.size()]);
            } else {
                msg.error_code = -3; return msg;
            }
        }
        msg.error_code = 0;
    } else {
        msg.error_code = -2;
    }
    return msg;
}

void FastIO::formatTrade(uint64_t qty, uint64_t price) {
    std::cout << "2," << qty << "," << (price / PRICE_MULTIPLIER) << "\n";
}

void FastIO::formatFilled(uint64_t id) {
    std::cout << "3," << id << "\n";
}

void FastIO::formatPartial(uint64_t id, uint64_t remaining_qty) {
    std::cout << "3," << id << "," << remaining_qty << "\n";
}