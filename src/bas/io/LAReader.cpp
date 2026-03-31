#include "LAReader.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

static const size_t blockCapacity = 4096;

LAReader::LAReader(size_t lookAheadCapacity)
    : m_lookAheadCapacity(lookAheadCapacity)
    , m_unlimit(lookAheadCapacity == 0)
    , m_pos(0)
    , m_limit(0)
{
    m_buffer.resize(
        m_unlimit ? blockCapacity : m_lookAheadCapacity);
}

LAReader::~LAReader() = default;

void LAReader::compact() {
    if (m_unlimit) return;
    if (m_pos > 0) {
        size_t del = m_pos;
        std::memmove(m_buffer.data(), m_buffer.data() + del,
                     (m_limit - del) * sizeof(char32_t));
        m_limit -= del;
        m_pos -= del;
    }
}

static size_t alignToNextPowerOfTwoChars(size_t value) {
    size_t power = 1;
    while (power < value)
        power <<= 1;
    return power;
}

bool LAReader::expandCapacity(size_t capacity) {
    if (m_unlimit) {
        size_t aligned = alignToNextPowerOfTwoChars(capacity);
        if (aligned > m_buffer.size())
            m_buffer.resize(aligned);
        return true;
    }
    return capacity == m_buffer.size();
}

bool LAReader::prefetch(size_t len) {
    size_t expectedCapacity = m_pos + len;
    expandCapacity(expectedCapacity);

    size_t capacity = m_buffer.size();
    size_t unread = m_limit - m_pos;
    if (unread >= len) // cache hit
        return true;

    if (len > capacity - m_pos)
        compact();

    size_t toRead = len - unread;
    size_t spare = capacity - m_limit;
    if (toRead > spare)
        toRead = spare;

    size_t n = 0;
    while (n < toRead) {
        int cp = readCharUnbuffered();
        if (cp < 0)
            break;
        m_buffer[m_limit++] = static_cast<char32_t>(cp);
        ++n;
    }
    return m_limit - m_pos >= len;
}

int LAReader::readChar() {
    if (!prefetch(1))
        return -1;
    char32_t cp = m_buffer[m_pos++];
    return static_cast<int>(cp);
}

std::string LAReader::readChars(size_t numCodePoints) {
    return Reader::readChars(numCodePoints);
}

std::string LAReader::readCharsUntilEOF() {
    return Reader::readCharsUntilEOF();
}

std::string LAReader::readLine() {
    return Reader::readLine();
}

std::string LAReader::readLineChopped() {
    return Reader::readLineChopped();
}

int64_t LAReader::skipChars(int64_t numCodePoints) {
    if (numCodePoints <= 0) return 0;
    size_t unread = m_limit - m_pos;
    if (static_cast<int64_t>(unread) >= numCodePoints) {
        m_pos += static_cast<size_t>(numCodePoints);
        return numCodePoints;
    }
    int64_t skipped = static_cast<int64_t>(unread);
    m_pos = 0;
    m_limit = 0;
    numCodePoints -= skipped;
    if (numCodePoints > 0) {
        size_t n = skipCharsUnbuffered(static_cast<size_t>(numCodePoints));
        skipped += static_cast<int64_t>(n);
    }
    return skipped;
}

int LAReader::lookAheadChar() {
    if (!prefetch(1))
        return -1;
    char32_t cp = m_buffer[m_pos];
    return static_cast<int>(cp);
}

size_t LAReader::lookAheadChars(char32_t* buf, size_t off, size_t len) {
    if (!buf || len == 0) return 0;
    prefetch(len);
    size_t avail = m_limit - m_pos;
    size_t toCopy = std::min(len, avail);
    for (size_t i = 0; i < toCopy; ++i)
        buf[off + i] = static_cast<char32_t>(m_buffer[m_pos + i]);
    return toCopy;
}

bool LAReader::rejectChars(int numCodePoints) {
    if (numCodePoints < 0 || static_cast<size_t>(numCodePoints) > m_pos)
        return false;
    m_pos -= static_cast<size_t>(numCodePoints);
    return true;
}
