#include "EncodingWriteStreamToOutputStream.hpp"

EncodingWriteStreamToOutputStream::EncodingWriteStreamToOutputStream(
    std::unique_ptr<OutputStream> out, std::string_view encoding)
    : EncodingWriteStream(encoding)
    , m_out(std::move(out))
    , m_closed(false)
{
}

size_t EncodingWriteStreamToOutputStream::_write(const uint8_t* buf, size_t len) {
    if (m_closed || !m_out || !buf) return 0;
    return m_out->write(buf, 0, len);
}

void EncodingWriteStreamToOutputStream::flush() {
    if (m_out && !m_closed) m_out->flush();
}

void EncodingWriteStreamToOutputStream::close() {
    if (!m_closed && m_out) {
        m_out->flush();
        m_out->close();
        m_closed = true;
    }
}
