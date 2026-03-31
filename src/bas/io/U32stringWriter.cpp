#include "U32stringWriter.hpp"

#include "../util/unicode.hpp"

#include <cstdint>

bool U32stringWriter::writeChar(char32_t codePoint) {
    if (codePoint < 0 || codePoint > 0x10FFFF) return false;
    m_buffer.push_back(static_cast<char32_t>(codePoint));
    return true;
}

bool U32stringWriter::write(std::string_view data) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
    const uint8_t* end = p + data.size();
    while (p < end) {
        auto [cp, consumed] = unicode::utf8DecodeCodePoint(p, end);
        if (consumed <= 0) return false;
        p += consumed;
        if (cp >= 0 && cp != -2) m_buffer.push_back(static_cast<char32_t>(cp));
    }
    return true;
}

bool U32stringWriter::writeln() {
    m_buffer.push_back(U'\n');
    return true;
}

bool U32stringWriter::writeln(std::string_view data) {
    if (!write(data)) return false;
    return writeln();
}

void U32stringWriter::flush() {}

void U32stringWriter::close() {}
