#include "WstringWriter.hpp"

#include "../util/unicode.hpp"

#include <cstdint>
#include <cwchar>

bool WstringWriter::writeChar(char32_t codePoint) {
    m_buffer.push_back(codePoint);
    return true;
}

bool WstringWriter::write(std::string_view data) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
    const uint8_t* end = p + data.size();
    while (p < end) {
        auto [cp, consumed] = unicode::utf8DecodeCodePoint(p, end);
        if (consumed <= 0) return false;
        p += consumed;
        if (cp >= 0 && cp != -2) writeChar(static_cast<char32_t>(cp));
    }
    return true;
}

bool WstringWriter::writeln() {
    m_buffer.push_back(L'\n');
    return true;
}

bool WstringWriter::writeln(std::string_view data) {
    if (!write(data)) return false;
    return writeln();
}

void WstringWriter::flush() {}

void WstringWriter::close() {}
