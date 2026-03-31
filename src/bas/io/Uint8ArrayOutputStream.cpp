#include "Uint8ArrayOutputStream.hpp"

#include <cstring>

Uint8ArrayOutputStream::Uint8ArrayOutputStream()
    : m_closed(false)
{
}

bool Uint8ArrayOutputStream::write(int byte) {
    if (m_closed) return false;
    m_data.push_back(static_cast<uint8_t>(byte & 0xFF));
    return true;
}

size_t Uint8ArrayOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !buf || len <= 0) {
        return 0;
    }
    m_data.insert(m_data.end(), buf + off, buf + off + len);
    return len;
}

void Uint8ArrayOutputStream::flush() {
    // No-op for in-memory buffer
}

void Uint8ArrayOutputStream::close() {
    m_closed = true;
}

void Uint8ArrayOutputStream::clear() {
    m_data.clear();
    m_closed = false;
}
