#include "U32stringReader.hpp"

#include "../util/unicode.hpp"

#include <cstdint>

static std::u32string utf8_to_u32(const std::string& utf8) {
    std::u32string out;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8.data());
    const uint8_t* end = p + utf8.size();
    while (p < end) {
        auto [cp, n] = unicode::utf8DecodeCodePoint(p, end);
        if (n <= 0 || cp < 0) break;
        out.push_back(static_cast<char32_t>(cp));
        p += n;
    }
    return out;
}

U32stringReader::U32stringReader(const std::string& data)
    : U32stringReader(utf8_to_u32(data))
{
}

U32stringReader::U32stringReader(const std::u32string& data)
    : m_data(data.data())
    , m_len(data.size())
    , m_pos(0)
{
}

int U32stringReader::readChar() {
    if (m_pos >= m_len) return -1;
    return static_cast<int>(m_data[m_pos++]);
}

std::string U32stringReader::readChars(size_t numCodePoints) {
    std::string out;
    out.reserve(numCodePoints * 4);
    for (size_t n = 0; n < numCodePoints && m_pos < m_len; ++n) {
        unicode::utf8EncodeCodePoint(static_cast<int>(m_data[m_pos++]), out);
    }
    return out;
}

std::string U32stringReader::readCharsUntilEOF() {
    std::string out;
    out.reserve((m_len - m_pos) * 4);
    while (m_pos < m_len) {
        unicode::utf8EncodeCodePoint(static_cast<int>(m_data[m_pos++]), out);
    }
    return out;
}

std::string U32stringReader::readLine() {
    size_t start = m_pos;
    while (m_pos < m_len && m_data[m_pos] != U'\n') ++m_pos;
    if (m_pos < m_len) ++m_pos; // include \n
    std::string out;
    out.reserve((m_pos - start) * 4);
    for (size_t i = start; i < m_pos; ++i) {
        unicode::utf8EncodeCodePoint(static_cast<int>(m_data[i]), out);
    }
    return out;
}

std::string U32stringReader::readLineChopped() {
    std::string line = readLine();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

int64_t U32stringReader::skipChars(int64_t len) {
    int64_t n = 0;
    while (n < len && m_pos < m_len) { ++m_pos; ++n; }
    return n;
}

void U32stringReader::close() {
    m_pos = m_len;
}

int64_t U32stringReader::charPosition() const {
    return static_cast<int64_t>(m_pos);
}

bool U32stringReader::seekChars(int64_t offset, std::ios::seekdir dir) {
    switch (dir) {
    case std::ios::beg:
        m_pos = static_cast<size_t>(offset);
        break;
    case std::ios::cur:
        m_pos += static_cast<size_t>(offset);
        break;
    case std::ios::end:
        m_pos = m_len - static_cast<size_t>(offset);
        break;
    }
    return true;
}
