#include "LAInputStreamFromStream.hpp"

#include <cstring>

LAInputStreamFromStream::LAInputStreamFromStream(std::unique_ptr<InputStream> in,
                                                 size_t lookAheadCapacity)
    : LAInputStream(lookAheadCapacity)
    , m_in(std::move(in))
{
}

int LAInputStreamFromStream::readUnbuffered() {
    return m_in ? m_in->read() : -1;
}

size_t LAInputStreamFromStream::readUnbuffered(uint8_t* buf, size_t off, size_t len) {
    if (!m_in || !buf || len == 0) return 0;
    size_t n = m_in->read(buf, off, len);
    return n;
}

size_t LAInputStreamFromStream::skipUnbuffered(size_t len) {
    if (!m_in || len == 0) return 0;
    int64_t n = m_in->skip(static_cast<int64_t>(len));
    return static_cast<size_t>(n >= 0 ? n : 0);
}

int64_t LAInputStreamFromStream::positionUnbuffered() const {
    ISeekable* s = dynamic_cast<ISeekable*>(m_in.get());
    return s ? s->position() : -1;
}

bool LAInputStreamFromStream::seekUnbuffered(int64_t offset, std::ios::seekdir dir) {
    ISeekable* s = dynamic_cast<ISeekable*>(m_in.get());
    return s && s->seek(offset, dir);
}

void LAInputStreamFromStream::close() {
    if (m_in) m_in->close();
}
