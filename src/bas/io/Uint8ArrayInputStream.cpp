#include "Uint8ArrayInputStream.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

Uint8ArrayInputStream::Uint8ArrayInputStream(const std::vector<uint8_t>& data,
                                           size_t off, size_t len)
    : m_owned()
    , m_ptr(data.data() + off)
    , m_len(len == npos ? (data.size() - off) : len)
    , m_position(0)
    , m_closed(false)
{
    if (off + m_len > data.size())
        m_len = off < data.size() ? data.size() - off : 0;
}

Uint8ArrayInputStream::Uint8ArrayInputStream(std::vector<uint8_t>&& data)
    : m_owned(std::move(data))
    , m_ptr(m_owned.data())
    , m_len(m_owned.size())
    , m_position(0)
    , m_closed(false)
{
}

Uint8ArrayInputStream::Uint8ArrayInputStream(const uint8_t* data, size_t len)
    : m_owned()
    , m_ptr(data)
    , m_len(len)
    , m_position(0)
    , m_closed(false)
{
}

int Uint8ArrayInputStream::read() {
    if (m_closed || m_position >= m_len) return -1;
    return static_cast<int>(m_ptr[m_position++]);
}

size_t Uint8ArrayInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !buf || len == 0) return 0;
    size_t remaining = m_len - m_position;
    if (remaining == 0) return 0;
    size_t toRead = std::min(len, remaining);
    std::memcpy(buf + off, m_ptr + m_position, toRead);
    m_position += toRead;
    return toRead;
}

void Uint8ArrayInputStream::close() {
    m_closed = true;
}

int64_t Uint8ArrayInputStream::skip(int64_t len) {
    if (m_closed || len <= 0) return 0;
    size_t remaining = m_len - m_position;
    if (remaining == 0) return 0;
    size_t toSkip = static_cast<size_t>(std::min(static_cast<int64_t>(remaining), len));
    m_position += toSkip;
    return static_cast<int64_t>(toSkip);
}

int64_t Uint8ArrayInputStream::position() const {
    return m_closed ? -1 : static_cast<int64_t>(m_position);
}

bool Uint8ArrayInputStream::canSeek(std::ios::seekdir dir) const {
    (void)dir;
    return !m_closed;
}

bool Uint8ArrayInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (m_closed) return false;
    int64_t pos = 0;
    switch (dir) {
    case std::ios::beg:
        pos = offset;
        break;
    case std::ios::cur:
        pos = static_cast<int64_t>(m_position) + offset;
        break;
    case std::ios::end:
        pos = static_cast<int64_t>(m_len) + offset;
        break;
    default:
        assert(false);
        return false;
    }
    if (pos < 0 || pos > static_cast<int64_t>(m_len)) return false;
    m_position = static_cast<size_t>(pos);
    return true;
}

std::vector<uint8_t> Uint8ArrayInputStream::data() const {
    return std::vector<uint8_t>(m_ptr, m_ptr + m_len);
}
