#include "LAInputStream.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

const size_t blockCapacity = 4096;

LAInputStream::LAInputStream(size_t lookAheadCapacity)
    : m_lookAheadCapacity(lookAheadCapacity)
    , m_unlimit(lookAheadCapacity == 0)
    , m_pos(0)
    , m_limit(0)
    , m_position(0)
{
    m_buffer.resize(
        m_unlimit ? blockCapacity : m_lookAheadCapacity);
}

void LAInputStream::compact() {
    if (m_unlimit) return;
    if (m_pos > 0) {
        size_t del = m_pos;
        std::memmove(m_buffer.data(), m_buffer.data() + del,
            m_limit - del);
        m_limit -= del;
        m_pos -= del;
    }
}

bool LAInputStream::prefetch(size_t len) {
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

    if (toRead == 1 ) {
        int b = readUnbuffered();
        if (b < 0)
            return false;
        m_buffer[m_limit++] = static_cast<uint8_t>(b);
        return true;
    } else {
        size_t n = readUnbuffered(m_buffer.data(), m_limit, toRead);
        m_limit += n;
    }
    return m_limit - m_pos >= len;
}

static size_t alignToNextPowerOfTwo(size_t value) {
    size_t power = 1;
    while (power < value)
        power <<= 1;
    return power;
}

bool LAInputStream::expandCapacity(size_t capacity) {
    if (m_unlimit) {
        size_t aligned = alignToNextPowerOfTwo(capacity);
        if (aligned > m_buffer.size())
            m_buffer.resize(aligned);
        return true;
    }
    return capacity == m_buffer.size();
}

int LAInputStream::read() {
    if (!prefetch(1))
        return -1;
    int b = static_cast<int>(m_buffer[m_pos++]);
    m_position++;
    return b;
}

size_t LAInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (!buf) return 0;
    size_t total = 0;
    while (total < len) {
        prefetch(len - total);
        size_t unread = m_limit - m_pos;
        if (unread == 0)
            break;
        size_t toCopy = std::min(len - total, unread);
        std::memcpy(buf + off, m_buffer.data() + m_pos, toCopy);
        m_pos += toCopy;
        off += toCopy;
        total += toCopy;
    }
    m_position += total;
    return total;
}

int LAInputStream::lookAhead() {
    if (!prefetch(1))
        return -1;
    return static_cast<int>(m_buffer[m_pos]);
}

size_t LAInputStream::lookAhead(uint8_t* buf, size_t off, size_t len) {
    if (!buf) return 0;
    prefetch(len);
    size_t avail = m_limit - m_pos;
    if (avail == 0)
        return 0;
    size_t toCopy = std::min(len, avail);
    std::memcpy(buf + off, m_buffer.data() + m_pos, toCopy);
    return toCopy;
}

std::vector<uint8_t> LAInputStream::readBytes(size_t len) {
    std::vector<uint8_t> out(len);
    size_t n = read(out.data(), 0, len);
    out.resize(n);
    return out;
}

std::vector<uint8_t> LAInputStream::readBytesUntilEOF() {
    std::vector<uint8_t> out;
    const int blockSize = 4096;
    // read by blocks
    while (true) {
        int b = read();
        if (b == -1) break;
        out.push_back(static_cast<uint8_t>(b));
    }
    return out;
}

std::vector<uint8_t> LAInputStream::readRawLine() {
    std::vector<uint8_t> line;
    while (true) {
        int b = read();
        if (b == -1) break;
        line.push_back(static_cast<uint8_t>(b));
        if (b == '\n') break;
    }
    return line;
}

std::vector<uint8_t> LAInputStream::readRawLineChopped() {
    std::vector<uint8_t> line = readRawLine();
    if (!line.empty() && line.back() == '\n') {
        line.pop_back();
        if (!line.empty() && line.back() == '\r') line.pop_back();
    }
    return line;
}

int64_t LAInputStream::position() const {
    return m_position;
}

bool LAInputStream::canSeek(std::ios::seekdir dir) const {
    switch (dir) {
    case std::ios::beg:
        return true;
    case std::ios::cur:
        return true;
    }
    return false;
}

bool LAInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    int64_t start = m_position - m_pos;
    if (start < 0) start = 0;
    int64_t end = m_position + m_limit - m_pos;
    int64_t delta = 0;

    switch (dir) {
    case std::ios::beg:
        if (offset >= start && offset < end)
            delta = offset - start;
        else
            return false;
        break;
    case std::ios::cur:
        delta = offset;
        break;
    default:
        return false;
    }

    if (delta < 0) { // backward seek/reject
        if (-delta < m_pos)
            return false;
    } else {        // forward seek/discard
        if (delta > m_limit - m_pos)
            return false;
    }
    m_pos += delta;
    m_position += delta;
    return true;
}

int64_t LAInputStream::skip(int64_t len) {
    if (len <= 0) return 0;
    size_t unread = m_limit - m_pos;
    if (unread >= len) {
        m_pos += len;
        m_position += len;
        return len;
    }
    
    int64_t skipped = unread;
    // assert: reach m_limit here
    // m_pos += unread;
    // so discard the whole buffer
    m_pos = 0;
    m_limit = 0;
    len -= unread;
    m_position += unread;

    // discard underlying data
    if (len > 0) {
        size_t n = skipUnbuffered(len);
        if (n < len) {
            // inconsistent state, ignore it
        }
        skipped += n;
        len -= n;
        m_position += n;
    }
    return skipped;
}

bool LAInputStream::reject(int len) {
    if (len < 0 || len > m_pos)
        return false;
    m_pos -= len;
    m_position -= len;
    return true;
}

void LAInputStream::discardPrefetch() {
    m_pos = 0;
    m_limit = 0;
}
