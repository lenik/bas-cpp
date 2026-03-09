#include "OutputStreamWriter.hpp"

#include <cstdint>
#include <stdexcept>

#include <unicode/umachine.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

OutputStreamWriter::OutputStreamWriter(std::unique_ptr<OutputStream> out, std::string_view charset)
    : m_out(std::move(out))
    , m_charset(charset)
    , m_encoder(charset)
    , m_outBuf(ByteBuffer::allocate(OUT_BUF_SIZE))
    , m_closed(false)
{
    if (!m_out) throw std::invalid_argument("OutputStreamWriter: null stream");
}

OutputStreamWriter::~OutputStreamWriter() {
    if (!m_closed && m_out) {
        flush();
        m_out->close();
        m_closed = true;
    }
}

void OutputStreamWriter::flushOutBuf() {
    if (!m_outBuf.hasRemaining()) return;
    m_outBuf.flip();
    while (m_outBuf.hasRemaining()) {
        if (!m_out->write(static_cast<int>(m_outBuf.get()))) break;
    }
    m_outBuf.clear();
}

bool OutputStreamWriter::writeChar(char32_t codePoint) {
    if (m_closed || !m_out) return false;
    std::vector<uint8_t> bytes = m_encoder.encodeChunkCodePoint(codePoint);
    for (uint8_t b : bytes) {
        if (!m_outBuf.hasRemaining())
            flushOutBuf();
        m_outBuf.put(b);
    }
    return true;
}

bool OutputStreamWriter::write(std::string_view data) {
    if (m_closed || !m_out) return false;
    if (data.empty()) return true;
    int32_t ulen = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strFromUTF8(nullptr, 0, &ulen, data.data(), static_cast<int32_t>(data.size()), &err);
    if (err != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(err)) return false;
    CharBuffer charBuf = CharBuffer::allocate(static_cast<size_t>(ulen) + 1);
    charBuf.clear();
    err = U_ZERO_ERROR;
    u_strFromUTF8(reinterpret_cast<UChar*>(charBuf.ptr()), static_cast<int32_t>(charBuf.remaining()), &ulen, data.data(), static_cast<int32_t>(data.size()), &err);
    if (U_FAILURE(err)) return false;
    charBuf.position(0).limit(static_cast<size_t>(ulen));
    while (charBuf.hasRemaining()) {
        if (!m_outBuf.hasRemaining())
            flushOutBuf();
        if (!m_encoder.encode(charBuf, m_outBuf)) break;
    }
    return true;
}

bool OutputStreamWriter::writeln() {
    return write("\n");
}

bool OutputStreamWriter::writeln(std::string_view data) {
    if (!write(data)) return false;
    return write("\n");
}

void OutputStreamWriter::flush() {
    if (m_closed || !m_out) return;
    flushOutBuf();
    m_outBuf.clear();
    m_encoder.encodeFlush(m_outBuf);
    m_outBuf.flip();
    while (m_outBuf.hasRemaining())
        m_out->write(static_cast<int>(m_outBuf.get()));
    m_out->flush();
}

void OutputStreamWriter::close() {
    if (!m_closed && m_out) {
        flush();
        m_out->close();
        m_closed = true;
    }
}
