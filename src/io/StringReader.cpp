#include "StringReader.hpp"

#include "../util/unicode.hpp"

StringReader::StringReader(std::string_view data)
    : m_data(data.data())
    , m_len(data.size())
    , m_pos(0)
{
}

int StringReader::readChar() {
    if (m_pos >= m_len) return -1;
    const char* ptr = m_data + m_pos;
    const char* end = m_data + m_len;
    auto [cp, consumed] = unicode::utf8DecodeCodePoint(
        reinterpret_cast<const uint8_t *>(ptr), 
        reinterpret_cast<const uint8_t *>(end));
    m_pos += static_cast<size_t>(consumed);
    if (cp == -2) return '?';
    return cp;
}

std::string StringReader::readChars(size_t numCodePoints) {
    if (numCodePoints == 0 || m_pos >= m_len) return {};
    std::string out;
    out.reserve(numCodePoints * 4);
    const char* ptr = m_data + m_pos;
    const char* end = m_data + m_len;
    for (size_t count = 0; count < numCodePoints && ptr < end; ) {
        auto [cp, consumed] = unicode::utf8DecodeCodePoint(
            reinterpret_cast<const uint8_t *>(ptr), 
            reinterpret_cast<const uint8_t *>(end));
        if (consumed <= 0) break;
        ptr += consumed;
        if (cp == -2) { unicode::utf8EncodeCodePoint('?', out); ++count; continue; }
        unicode::utf8EncodeCodePoint(cp, out);
        ++count;
    }
    m_pos = static_cast<size_t>(ptr - m_data);
    return out;
}

std::string StringReader::readCharsUntilEOF() {
    if (m_pos >= m_len) return {};
    std::string out(reinterpret_cast<const char*>(m_data + m_pos), m_len - m_pos);
    m_pos = m_len;
    return out;
}

std::string StringReader::readLine() {
    const char* ptr = m_data + m_pos;
    const char* end = m_data + m_len;
    const char* start = ptr;
    while (ptr < end && *ptr != '\n') ++ptr;
    if (ptr < end) ++ptr; // include \n
    m_pos = static_cast<size_t>(ptr - m_data);
    return std::string(reinterpret_cast<const char*>(start), static_cast<size_t>(ptr - start));
}

std::string StringReader::readLineChopped() {
    std::string line = readLine();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

int64_t StringReader::skipChars(int64_t len) {
    if (len <= 0 || m_pos >= m_len) return 0;
    int64_t n = 0;
    const char* ptr = m_data + m_pos;
    const char* end = m_data + m_len;
    while (n < len && ptr < end) {
        auto [cp, consumed] = unicode::utf8DecodeCodePoint(
            reinterpret_cast<const uint8_t *>(ptr), 
            reinterpret_cast<const uint8_t *>(end));
        if (consumed <= 0) break;
        ptr += consumed;
        ++n;
    }
    m_pos = static_cast<size_t>(ptr - m_data);
    return n;
}

void StringReader::close() {
    m_pos = m_len;
}
