#include "BufferedReader.hpp"

#include "../util/unicode.hpp"

#include <cstdint>

BufferedReader::BufferedReader(std::unique_ptr<Reader> in)
    : m_in(std::move(in))
    , m_pos(0)
    , m_closed(false)
{
    if (!m_in) throw std::invalid_argument("BufferedReader: null Reader");
}

BufferedReader::~BufferedReader() {
    if (!m_closed && m_in) m_in->close();
}

void BufferedReader::refill() {
    if (!m_in || m_closed) return;
    m_buf = m_in->readChars(BUFFER_CODE_POINTS);
    m_pos = 0;
}

int BufferedReader::readChar() {
    if (m_pos >= m_buf.size())
        refill();
    if (m_buf.empty())
        return -1;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(m_buf.data()) + m_pos;
    const uint8_t* end = reinterpret_cast<const uint8_t*>(m_buf.data()) + m_buf.size();
    auto [cp, consumed] = unicode::utf8DecodeCodePoint(ptr, end);
    m_pos += static_cast<size_t>(consumed);
    if (cp == -2) return '?';
    return cp;
}

std::string BufferedReader::readChars(size_t numCodePoints) {
    if (numCodePoints == 0) return {};
    std::string out;
    out.reserve(numCodePoints * 4);
    for (size_t count = 0; count < numCodePoints; ) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
        ++count;
    }
    return out;
}

std::string BufferedReader::readCharsUntilEOF() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

std::string BufferedReader::readLine() {
    return Reader::readLine();
}

std::string BufferedReader::readLineChopped() {
    return Reader::readLineChopped();
}

int64_t BufferedReader::skipChars(int64_t numCodePoints) {
    if (numCodePoints <= 0) return 0;
    return Reader::skipChars(numCodePoints);
}

void BufferedReader::close() {
    if (!m_closed && m_in) {
        m_in->close();
        m_closed = true;
    }
}
