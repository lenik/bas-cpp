#include "CharsetDecoder.hpp"

#include <cstring>
#include <stdexcept>

#include <unicode/ucnv.h>
#include <unicode/umachine.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

CharsetDecoder::CharsetDecoder(std::string_view charset)
    : m_charset(charset)
    , m_cnv(nullptr)
{
    UErrorCode err = U_ZERO_ERROR;
    std::string name(m_charset);
    m_cnv = ucnv_open(name.c_str(), &err);
    if (U_FAILURE(err) || !m_cnv) {
        if (m_cnv) ucnv_close(m_cnv);
        m_cnv = nullptr;
        throw std::runtime_error("CharsetDecoder: unsupported charset");
    }
}

CharsetDecoder::~CharsetDecoder() {
    if (m_cnv) {
        ucnv_close(m_cnv);
        m_cnv = nullptr;
    }
}

CharsetDecoder::CharsetDecoder(CharsetDecoder&& other) noexcept
    : m_charset(std::move(other.m_charset))
    , m_cnv(other.m_cnv)
{
    other.m_cnv = nullptr;
}

CharsetDecoder& CharsetDecoder::operator=(CharsetDecoder&& other) noexcept {
    if (this != &other) {
        if (m_cnv) ucnv_close(m_cnv);
        m_charset = std::move(other.m_charset);
        m_cnv = other.m_cnv;
        other.m_cnv = nullptr;
    }
    return *this;
}

void CharsetDecoder::reset() {
    if (m_cnv) ucnv_resetToUnicode(m_cnv);
}

std::string CharsetDecoder::decode(const uint8_t* src, size_t srcLen) {
    if (!m_cnv || !src) return srcLen ? std::string() : "";
    reset();
    std::string out = decodeChunk(src, srcLen);
    std::string flush = decodeFlush();
    out.append(flush);
    return out;
}

std::string CharsetDecoder::decode(std::string_view src) {
    return decode(reinterpret_cast<const uint8_t*>(src.data()), src.size());
}

std::string CharsetDecoder::decodeChunk(const uint8_t* src, size_t srcLen) {
    if (!m_cnv || !src || srcLen == 0) return "";
    ByteBuffer inBuf = ByteBuffer::allocate(srcLen + 16);
    inBuf.clear().put(src, srcLen).flip();
    size_t ucap = srcLen * 2 + 16;
    CharBuffer outBuf = CharBuffer::allocate(ucap);
    outBuf.clear();
    decode(inBuf, outBuf);
    size_t ulen = outBuf.position();
    if (ulen == 0) return "";
    outBuf.flip();
    int32_t utf8Cap = static_cast<int32_t>(ulen * 4 + 16);
    std::string result;
    result.resize(static_cast<size_t>(utf8Cap));
    int32_t utf8Len = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strToUTF8(&result[0], utf8Cap, &utf8Len, reinterpret_cast<const UChar*>(outBuf.data()), static_cast<int32_t>(ulen), &err);
    if (err == U_BUFFER_OVERFLOW_ERROR) {
        result.resize(static_cast<size_t>(utf8Len));
        err = U_ZERO_ERROR;
        u_strToUTF8(&result[0], utf8Len, &utf8Len, reinterpret_cast<const UChar*>(outBuf.data()), static_cast<int32_t>(ulen), &err);
    }
    if (U_FAILURE(err)) return "";
    result.resize(static_cast<size_t>(utf8Len));
    return result;
}

std::string CharsetDecoder::decodeFlush() {
    if (!m_cnv) return "";
    CharBuffer outBuf = CharBuffer::allocate(64);
    outBuf.clear();
    if (!decodeFlush(outBuf)) return "";
    size_t ulen = outBuf.position();
    if (ulen == 0) return "";
    outBuf.flip();
    int32_t utf8Cap = static_cast<int32_t>(ulen * 4 + 16);
    std::string result;
    result.resize(static_cast<size_t>(utf8Cap));
    int32_t utf8Len = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strToUTF8(&result[0], utf8Cap, &utf8Len, reinterpret_cast<const UChar*>(outBuf.data()), static_cast<int32_t>(ulen), &err);
    if (err == U_BUFFER_OVERFLOW_ERROR) {
        result.resize(static_cast<size_t>(utf8Len));
        err = U_ZERO_ERROR;
        u_strToUTF8(&result[0], utf8Len, &utf8Len, reinterpret_cast<const UChar*>(outBuf.data()), static_cast<int32_t>(ulen), &err);
    }
    if (U_FAILURE(err)) return "";
    result.resize(static_cast<size_t>(utf8Len));
    return result;
}

bool CharsetDecoder::decode(ByteBuffer& in, CharBuffer& out) {
    if (!m_cnv || !in.hasRemaining()) return false;
    const char* source = reinterpret_cast<const char*>(in.ptr());
    const char* sourceLimit = reinterpret_cast<const char*>(in.ptr()) + in.remaining();
    UChar* target = reinterpret_cast<UChar*>(out.ptr());
    UChar* targetLimit = reinterpret_cast<UChar*>(out.ptr()) + out.remaining();
    UErrorCode err = U_ZERO_ERROR;
    ucnv_toUnicode(m_cnv, &target, targetLimit, &source, sourceLimit, nullptr, false, &err);
    in.position(static_cast<size_t>(source - reinterpret_cast<const char*>(in.data())));
    out.position(static_cast<size_t>(target - reinterpret_cast<UChar*>(out.data())));
    return in.position() > 0 || out.position() > 0;
}

bool CharsetDecoder::decodeFlush(CharBuffer& out) {
    if (!m_cnv) return false;
    const char* source = nullptr;
    const char* sourceLimit = nullptr;
    UChar* target = reinterpret_cast<UChar*>(out.ptr());
    UChar* targetLimit = reinterpret_cast<UChar*>(out.ptr()) + out.remaining();
    UErrorCode err = U_ZERO_ERROR;
    ucnv_toUnicode(m_cnv, &target, targetLimit, &source, sourceLimit, nullptr, true, &err);
    out.position(static_cast<size_t>(target - reinterpret_cast<UChar*>(out.data())));
    return out.position() > 0;
}

bool CharsetDecoder::decode(ByteBuffer& in, Char32Buffer& out) {
    if (!m_cnv || !in.hasRemaining() || !out.hasRemaining()) return false;
    size_t u16cap = in.remaining() * 2 + 16;
    CharBuffer u16buf = CharBuffer::allocate(u16cap);
    u16buf.clear();
    if (!decode(in, u16buf)) return false;
    u16buf.flip();
    if (!u16buf.hasRemaining()) return true;
    int32_t destLen = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strToUTF32(reinterpret_cast<UChar32*>(out.ptr()), static_cast<int32_t>(out.remaining()), &destLen,
                 reinterpret_cast<const UChar*>(u16buf.ptr()), static_cast<int32_t>(u16buf.remaining()), &err);
    if (destLen > 0)
        out.position(out.position() + static_cast<size_t>(destLen));
    return true;
}

bool CharsetDecoder::decodeFlush(Char32Buffer& out) {
    if (!m_cnv || !out.hasRemaining()) return false;
    CharBuffer u16buf = CharBuffer::allocate(64);
    u16buf.clear();
    if (!decodeFlush(u16buf)) return false;
    u16buf.flip();
    if (!u16buf.hasRemaining()) return false;
    int32_t destLen = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strToUTF32(reinterpret_cast<UChar32*>(out.ptr()), static_cast<int32_t>(out.remaining()), &destLen,
                 reinterpret_cast<const UChar*>(u16buf.ptr()), static_cast<int32_t>(u16buf.remaining()), &err);
    if (destLen > 0)
        out.position(out.position() + static_cast<size_t>(destLen));
    return true;
}
