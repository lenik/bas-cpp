#include "ConstBufferInputStream.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

ConstBufferInputStream::ConstBufferInputStream(const uint8_t* data, size_t len)
    : m_data(data)
    , m_len(len)
    , m_position(0)
    , m_closed(false)
{
}

ConstBufferInputStream::ConstBufferInputStream(std::string_view data)
    : ConstBufferInputStream(reinterpret_cast<const uint8_t*>(data.data()), data.size())
{
}

ConstBufferInputStream::~ConstBufferInputStream() = default;

int ConstBufferInputStream::read() {
    if (m_closed || m_position >= m_len) return -1;
    return static_cast<int>(m_data[m_position++]);
}

size_t ConstBufferInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !buf || len == 0) return 0;
    size_t remaining = m_len - m_position;
    if (remaining == 0) return 0;
    size_t toRead = std::min(len, remaining);
    std::memcpy(buf + off, m_data + m_position, toRead);
    m_position += toRead;
    return toRead;
}

void ConstBufferInputStream::close() {
    m_closed = true;
}

int64_t ConstBufferInputStream::skip(int64_t len) {
    if (m_closed || len <= 0) return 0;
    size_t remaining = m_len - m_position;
    if (remaining == 0) return 0;
    size_t toSkip = static_cast<size_t>(std::min(static_cast<int64_t>(remaining), len));
    m_position += toSkip;
    return static_cast<int64_t>(toSkip);
}

int64_t ConstBufferInputStream::position() const {
    return m_closed ? -1 : static_cast<int64_t>(m_position);
}

bool ConstBufferInputStream::canSeek(std::ios::seekdir dir) const {
    (void)dir;
    return !m_closed;
}

bool ConstBufferInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (m_closed) return false;
    int64_t pos = 0;
    switch (dir) {
    default:
        assert(false);
        [[fallthrough]];
    case std::ios::beg:
        pos = offset;
        break;
    case std::ios::cur:
        pos = static_cast<int64_t>(m_position) + offset;
        break;
    case std::ios::end:
        pos = static_cast<int64_t>(m_len) + offset;
        break;
    }
    if (pos < 0 || pos > static_cast<int64_t>(m_len)) return false;
    m_position = static_cast<size_t>(pos);
    return true;
}
