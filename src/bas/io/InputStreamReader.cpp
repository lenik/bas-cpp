#include "InputStreamReader.hpp"

#include "../util/unicode.hpp"

#include <cstdint>
#include <stdexcept>

#include <unicode/umachine.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

InputStreamReader::InputStreamReader(std::unique_ptr<InputStream> in, std::string_view charset)
    : m_in(std::move(in))
    , m_charset(charset)
    , m_decoder(charset)
    , m_inBuf(ByteBuffer::allocate(READ_BUF_SIZE))
    , m_charBuf(CharBuffer::allocate(CHAR_BUF_SIZE))
    , m_decodedPos(0)
    , m_closed(false)
    , m_eof(false)
{
    if (!m_in) throw std::invalid_argument("InputStreamReader: null stream");
}

InputStreamReader::~InputStreamReader() {
    if (!m_closed && m_in) m_in->close();
}

bool InputStreamReader::fillDecodedBuffer() {
    if (m_eof || !m_in) return false;
    if (m_decodedPos < m_decoded.size()) return true;
    m_decoded.clear();
    m_decodedPos = 0;
    m_inBuf.compact();
    if (m_inBuf.remaining() == 0)
        m_inBuf.clear();
    size_t rem = m_inBuf.position();
    size_t space = m_inBuf.remaining();
    size_t n = m_in->read(m_inBuf.data(), rem, space);
    if (n < space)
        m_eof = true;
    if (n == 0 && rem == 0) {
        m_charBuf.clear();
        m_decoder.decodeFlush(m_charBuf);
        m_charBuf.flip();
        if (!m_charBuf.hasRemaining()) return false;
        int32_t utf8Len = 0;
        UErrorCode err = U_ZERO_ERROR;
        u_strToUTF8(nullptr, 0, &utf8Len, reinterpret_cast<const UChar*>(m_charBuf.ptr()), static_cast<int32_t>(m_charBuf.remaining()), &err);
        if (utf8Len > 0) {
            m_decoded.resize(static_cast<size_t>(utf8Len));
            err = U_ZERO_ERROR;
            u_strToUTF8(&m_decoded[0], utf8Len, &utf8Len, reinterpret_cast<const UChar*>(m_charBuf.ptr()), static_cast<int32_t>(m_charBuf.remaining()), &err);
        }
        return !m_decoded.empty();
    }
    m_inBuf.limit(rem + n).position(0);
    m_charBuf.clear();
    m_decoder.decode(m_inBuf, m_charBuf);
    if (m_eof)
        m_decoder.decodeFlush(m_charBuf);
    m_inBuf.compact();
    m_charBuf.flip();
    if (!m_charBuf.hasRemaining()) return false;
    int32_t utf8Len = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strToUTF8(nullptr, 0, &utf8Len, reinterpret_cast<const UChar*>(m_charBuf.ptr()), static_cast<int32_t>(m_charBuf.remaining()), &err);
    if (err == U_BUFFER_OVERFLOW_ERROR && utf8Len > 0) {
        m_decoded.resize(static_cast<size_t>(utf8Len));
        err = U_ZERO_ERROR;
        u_strToUTF8(&m_decoded[0], utf8Len, &utf8Len, reinterpret_cast<const UChar*>(m_charBuf.ptr()), static_cast<int32_t>(m_charBuf.remaining()), &err);
    }
    return !m_decoded.empty();
}

int InputStreamReader::readChar() {
    if (m_closed) return -1;
    if (m_decodedPos >= m_decoded.size()) {
        size_t n = fillDecodedBuffer();
        if (!n) return -1;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(m_decoded.data());
    const uint8_t* ptr = data + m_decodedPos;
    const uint8_t* end = data + m_decoded.size();
    auto [cp, consumed] = unicode::utf8DecodeCodePoint(ptr, end);
    m_decodedPos += static_cast<size_t>(consumed);
    if (cp == -2) return '?';
    return cp;
}

std::string InputStreamReader::readChars(size_t numCodePoints) {
    std::string out;
    out.reserve(numCodePoints * 4);
    for (size_t count = 0; count < numCodePoints; ) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
        ++count;
    }
    return out;
}

std::string InputStreamReader::readCharsUntilEOF() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

std::string InputStreamReader::readLine() {
    return Reader::readLine();
}

std::string InputStreamReader::readLineChopped() {
    return Reader::readLineChopped();
}

int64_t InputStreamReader::skipChars(int64_t numCodePoints) {
    return Reader::skipChars(numCodePoints);
}

void InputStreamReader::close() {
    if (!m_closed && m_in) {
        m_in->close();
        m_closed = true;
    }
}
