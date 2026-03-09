#include "Char32Buffer.hpp"

Char32Buffer Char32Buffer::allocate(size_t capacity) {
    return Char32Buffer(capacity);
}

Char32Buffer::Char32Buffer(size_t capacity)
    : m_data(capacity, 0)
    , m_position(0)
    , m_limit(capacity)
{
}

Char32Buffer& Char32Buffer::position(size_t p) {
    if (p > m_limit) p = m_limit;
    m_position = p;
    return *this;
}

Char32Buffer& Char32Buffer::limit(size_t l) {
    if (l > m_data.size()) l = m_data.size();
    m_limit = l;
    if (m_position > m_limit) m_position = m_limit;
    return *this;
}

Char32Buffer& Char32Buffer::clear() {
    m_position = 0;
    m_limit = m_data.size();
    return *this;
}

Char32Buffer& Char32Buffer::flip() {
    m_limit = m_position;
    m_position = 0;
    return *this;
}

Char32Buffer& Char32Buffer::rewind() {
    m_position = 0;
    return *this;
}

Char32Buffer& Char32Buffer::compact() {
    size_t rem = m_limit - m_position;
    if (rem > 0 && m_position > 0)
        for (size_t i = 0; i < rem; ++i)
            m_data[i] = m_data[m_position + i];
    m_position = rem;
    m_limit = m_data.size();
    return *this;
}

char32_t Char32Buffer::get() {
    if (m_position >= m_limit) return 0;
    return m_data[m_position++];
}

Char32Buffer& Char32Buffer::get(char32_t* dst, size_t len) {
    size_t n = remaining();
    if (len > n) len = n;
    for (size_t i = 0; i < len; ++i)
        dst[i] = m_data[m_position++];
    return *this;
}

Char32Buffer& Char32Buffer::put(char32_t c) {
    if (m_position < m_limit)
        m_data[m_position++] = c;
    return *this;
}

Char32Buffer& Char32Buffer::put(const char32_t* src, size_t len) {
    size_t n = remaining();
    if (len > n) len = n;
    for (size_t i = 0; i < len; ++i)
        m_data[m_position++] = src[i];
    return *this;
}
