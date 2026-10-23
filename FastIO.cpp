#include "FastIO.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint64_t PRICE_MULTIPLIER = 10000;
constexpr size_t FRACTION_DIGITS = 4;
constexpr size_t OUTPUT_BUFFER_SIZE = 64 * 1024;
constexpr std::array<uint64_t, FRACTION_DIGITS + 1> POWERS_OF_10 = {
    10000, 1000, 100, 10, 1
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input) : input_(input) {}

    bool next(std::string_view& token) noexcept {
        if (pos_ > input_.size()) return false;

        const size_t comma = input_.find(',', pos_);
        if (comma == std::string_view::npos) {
            token = input_.substr(pos_);
            pos_ = input_.size() + 1;
        } else {
            token = input_.substr(pos_, comma - pos_);
            pos_ = comma + 1;
        }
        return true;
    }

private:
    std::string_view input_;
    size_t pos_ = 0;
};

bool parseUint64(std::string_view token, uint64_t& value) noexcept {
    if (token.empty()) return false;
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

bool parseInt(std::string_view token, int& value) noexcept {
    if (token.empty()) return false;
    const char* begin = token.data();
    const char* end = begin + token.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc() && result.ptr == end;
}

bool parsePrice(std::string_view token, uint64_t& price) noexcept {
    if (token.empty()) return false;

    const size_t dot = token.find('.');
    if (dot != std::string_view::npos &&
        token.find('.', dot + 1) != std::string_view::npos) {
        return false;
    }

    if (dot == std::string_view::npos) {
        uint64_t integer_part = 0;
        if (!parseUint64(token, integer_part)) return false;
        if (integer_part > (MAX_PRICE_RANGE - 1) / PRICE_MULTIPLIER) {
            return false;
        }
        price = integer_part * PRICE_MULTIPLIER;
        return price < MAX_PRICE_RANGE;
    }

    const std::string_view integer_str = token.substr(0, dot);
    const std::string_view fraction_str = token.substr(dot + 1);
    if (integer_str.empty() || fraction_str.empty() ||
        fraction_str.size() > FRACTION_DIGITS) {
        return false;
    }

    uint64_t integer_part = 0;
    uint64_t fractional_part = 0;
    if (!parseUint64(integer_str, integer_part) ||
        !parseUint64(fraction_str, fractional_part)) {
        return false;
    }
    if (integer_part > (MAX_PRICE_RANGE - 1) / PRICE_MULTIPLIER) {
        return false;
    }

    price = integer_part * PRICE_MULTIPLIER +
            fractional_part * POWERS_OF_10[fraction_str.size()];
    return price < MAX_PRICE_RANGE;
}

void defaultSink(const char* data, size_t size) {
    if (size != 0) std::fwrite(data, 1, size, stdout);
}

FastIO::WriteFn active_sink = defaultSink;
bool sink_is_default = true;
std::array<char, OUTPUT_BUFFER_SIZE> output_buffer;
size_t output_used = 0;

void flushBuffer() {
    if (output_used == 0) return;
    active_sink(output_buffer.data(), output_used);
    output_used = 0;
}

void appendRaw(const char* data, size_t size) {
    while (size > 0) {
        if (output_used == OUTPUT_BUFFER_SIZE) flushBuffer();
        const size_t available = OUTPUT_BUFFER_SIZE - output_used;
        const size_t chunk = size < available ? size : available;
        std::memcpy(output_buffer.data() + output_used, data, chunk);
        output_used += chunk;
        data += chunk;
        size -= chunk;
    }
}

void appendChar(char value) {
    if (output_used == OUTPUT_BUFFER_SIZE) flushBuffer();
    output_buffer[output_used++] = value;
}

void appendUint64(uint64_t value) {
    char buffer[20];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    appendRaw(buffer, static_cast<size_t>(result.ptr - buffer));
}

void appendPrice(uint64_t price) {
    appendUint64(price / PRICE_MULTIPLIER);

    uint64_t fraction = price % PRICE_MULTIPLIER;
    if (fraction == 0) return;

    char digits[FRACTION_DIGITS];
    for (size_t i = FRACTION_DIGITS; i > 0; --i) {
        digits[i - 1] = static_cast<char>('0' + fraction % 10);
        fraction /= 10;
    }

    size_t length = FRACTION_DIGITS;
    while (length > 1 && digits[length - 1] == '0') --length;

    appendChar('.');
    appendRaw(digits, length);
}

} // namespace

ParsedMessage FastIO::parseLine(std::string_view line) {
    ParsedMessage msg;

    // Be tolerant of CRLF input on Windows while keeping the grammar strict.
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    Tokenizer tokenizer(line);
    std::string_view token;

    if (!tokenizer.next(token) || token.empty()) {
        msg.error_code = -1;
        return msg;
    }

    int type = 0;
    if (!parseInt(token, type)) {
        msg.error_code = -3;
        return msg;
    }

    if (type == 1) { // Cancel: Type, ID
        if (!tokenizer.next(token) || token.empty()) {
            msg.error_code = -1;
            return msg;
        }
        if (!parseUint64(token, msg.id)) {
            msg.error_code = -3;
            return msg;
        }
        if (tokenizer.next(token)) {
            msg.error_code = -1;
            return msg;
        }
        msg.error_code = 1;
        return msg;
    }

    if (type != 0) {
        msg.error_code = -2;
        return msg;
    }

    // Add: Type, ID, Side, Quantity, Price
    if (!tokenizer.next(token) || token.empty()) {
        msg.error_code = -1;
        return msg;
    }
    if (!parseUint64(token, msg.id)) {
        msg.error_code = -3;
        return msg;
    }

    if (!tokenizer.next(token) || token.empty()) {
        msg.error_code = -1;
        return msg;
    }
    int side = 0;
    if (!parseInt(token, side) || (side != 0 && side != 1)) {
        msg.error_code = -3;
        return msg;
    }
    msg.side = static_cast<Side>(side);

    if (!tokenizer.next(token) || token.empty()) {
        msg.error_code = -1;
        return msg;
    }
    if (!parseUint64(token, msg.quantity) || msg.quantity == 0) {
        msg.error_code = -3;
        return msg;
    }

    if (!tokenizer.next(token) || token.empty()) {
        msg.error_code = -1;
        return msg;
    }
    if (!parsePrice(token, msg.price)) {
        msg.error_code = -3;
        return msg;
    }

    if (tokenizer.next(token)) {
        msg.error_code = -1;
        return msg;
    }

    msg.error_code = 0;
    return msg;
}

void FastIO::formatTrade(uint64_t qty, uint64_t price) {
    appendChar('2');
    appendChar(',');
    appendUint64(qty);
    appendChar(',');
    appendPrice(price);
    appendChar('\n');
}

void FastIO::formatFilled(uint64_t id) {
    appendChar('3');
    appendChar(',');
    appendUint64(id);
    appendChar('\n');
}

void FastIO::formatPartial(uint64_t id, uint64_t remaining_qty) {
    appendChar('3');
    appendChar(',');
    appendUint64(id);
    appendChar(',');
    appendUint64(remaining_qty);
    appendChar('\n');
}

void FastIO::setSink(WriteFn sink) noexcept {
    flushBuffer();
    active_sink = sink != nullptr ? sink : defaultSink;
    sink_is_default = active_sink == defaultSink;
}

void FastIO::flush() {
    flushBuffer();
    if (sink_is_default) std::fflush(stdout);
}
