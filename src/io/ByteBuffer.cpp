#include "ByteBuffer.hpp"

ByteBuffer ByteBuffer::allocate(size_t capacity) {
    return ByteBuffer(capacity);
}

ByteBuffer::ByteBuffer(size_t capacity)
    : m_data(capacity, 0)
    , m_position(0)
    , m_limit(capacity)
{
}

ByteBuffer& ByteBuffer::position(size_t p) {
    if (p > m_limit) p = m_limit;
    m_position = p;
    return *this;
}

ByteBuffer& ByteBuffer::limit(size_t l) {
    if (l > m_data.size()) l = m_data.size();
    m_limit = l;
    if (m_position > m_limit) m_position = m_limit;
    return *this;
}

ByteBuffer& ByteBuffer::clear() {
    m_position = 0;
    m_limit = m_data.size();
    return *this;
}

ByteBuffer& ByteBuffer::flip() {
    m_limit = m_position;
    m_position = 0;
    return *this;
}

ByteBuffer& ByteBuffer::rewind() {
    m_position = 0;
    return *this;
}

ByteBuffer& ByteBuffer::compact() {
    size_t rem = m_limit - m_position;
    if (rem > 0 && m_position > 0)
        for (size_t i = 0; i < rem; ++i)
            m_data[i] = m_data[m_position + i];
    m_position = rem;
    m_limit = m_data.size();
    return *this;
}

uint8_t ByteBuffer::get() {
    if (m_position >= m_limit) return 0;
    return m_data[m_position++];
}

ByteBuffer& ByteBuffer::get(uint8_t* dst, size_t len) {
    size_t n = remaining();
    if (len > n) len = n;
    for (size_t i = 0; i < len; ++i)
        dst[i] = m_data[m_position++];
    return *this;
}

ByteBuffer& ByteBuffer::put(uint8_t b) {
    if (m_position < m_limit)
        m_data[m_position++] = b;
    return *this;
}

ByteBuffer& ByteBuffer::put(const uint8_t* src, size_t len) {
    size_t n = remaining();
    if (len > n) len = n;
    for (size_t i = 0; i < len; ++i)
        m_data[m_position++] = src[i];
    return *this;
}
