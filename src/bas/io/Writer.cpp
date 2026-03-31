#include "Writer.hpp"

#include "../util/unicode.hpp"

#include <cstdint>
#include <string_view>

bool Writer::write(std::string_view data) {
    if (data.empty()) return true;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
    const uint8_t* end = p + data.size();
    while (p < end) {
        auto [cp, consumed] = unicode::utf8DecodeCodePoint(p, end);
        if (consumed == 0) break;
        p += consumed;
        if (cp == -2) return false;  // malformed
        if (cp >= 0 && !writeChar(cp)) return false;
    }
    return true;
}

bool Writer::writeln() {
    return writeChar('\n');
}

bool Writer::writeln(std::string_view data) {
    if (!write(data)) return false;
    return writeln();
}

void Writer::flush() {}

void Writer::close() {}
