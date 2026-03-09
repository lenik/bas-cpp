#include "CharsetEncoder.hpp"

#include <cstring>
#include <stdexcept>

#include <unicode/ucnv.h>
#include <unicode/umachine.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

namespace {
std::vector<uint8_t> encodeUChars(UConverter* cnv, const UChar* uchars, int32_t ulen, bool flush) {
    if (!cnv || !uchars || ulen <= 0) return {};
    size_t bcap = static_cast<size_t>(ulen) * 4 + 32;
    std::vector<uint8_t> out(bcap);
    const UChar* source = uchars;
    const UChar* sourceLimit = uchars + ulen;
    char* target = reinterpret_cast<char*>(out.data());
    char* targetLimit = target + bcap;
    UErrorCode err = U_ZERO_ERROR;
    ucnv_fromUnicode(cnv, &target, targetLimit, &source, sourceLimit, nullptr, flush, &err);
    if (err == U_BUFFER_OVERFLOW_ERROR) {
        bcap = static_cast<size_t>(target - reinterpret_cast<char*>(out.data())) + 64;
        out.resize(bcap);
        target = reinterpret_cast<char*>(out.data());
        targetLimit = target + bcap;
        source = uchars;
        sourceLimit = uchars + ulen;
        err = U_ZERO_ERROR;
        ucnv_fromUnicode(cnv, &target, targetLimit, &source, sourceLimit, nullptr, flush, &err);
    }
    out.resize(static_cast<size_t>(target - reinterpret_cast<char*>(out.data())));
    return out;
}
}

CharsetEncoder::CharsetEncoder(std::string_view charset)
    : m_charset(charset)
    , m_cnv(nullptr)
{
    UErrorCode err = U_ZERO_ERROR;
    std::string name(m_charset);
    m_cnv = ucnv_open(name.c_str(), &err);
    if (U_FAILURE(err) || !m_cnv) {
        if (m_cnv) ucnv_close(m_cnv);
        m_cnv = nullptr;
        throw std::runtime_error("CharsetEncoder: unsupported charset");
    }
}

CharsetEncoder::~CharsetEncoder() {
    if (m_cnv) {
        ucnv_close(m_cnv);
        m_cnv = nullptr;
    }
}

CharsetEncoder::CharsetEncoder(CharsetEncoder&& other) noexcept
    : m_charset(std::move(other.m_charset))
    , m_cnv(other.m_cnv)
{
    other.m_cnv = nullptr;
}

CharsetEncoder& CharsetEncoder::operator=(CharsetEncoder&& other) noexcept {
    if (this != &other) {
        if (m_cnv) ucnv_close(m_cnv);
        m_charset = std::move(other.m_charset);
        m_cnv = other.m_cnv;
        other.m_cnv = nullptr;
    }
    return *this;
}

std::vector<uint8_t> CharsetEncoder::encode(std::string_view utf8) {
    if (!m_cnv) return {};
    reset();
    std::vector<uint8_t> out = encodeChunk(utf8);
    std::vector<uint8_t> flush = encodeFlush();
    out.insert(out.end(), flush.begin(), flush.end());
    return out;
}

std::vector<uint8_t> CharsetEncoder::encodeCodePoint(int codePoint) {
    if (codePoint < 0) return {};
    std::string utf8;
    if (codePoint < 0x80) {
        utf8.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        utf8.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        utf8.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    reset();
    std::vector<uint8_t> out = encodeChunk(utf8);
    std::vector<uint8_t> flush = encodeFlush();
    out.insert(out.end(), flush.begin(), flush.end());
    return out;
}

std::vector<uint8_t> CharsetEncoder::encodeChunk(std::string_view utf8) {
    if (!m_cnv || utf8.empty()) return {};
    int32_t ulen = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strFromUTF8(nullptr, 0, &ulen, utf8.data(), static_cast<int32_t>(utf8.size()), &err);
    if (err != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(err)) return {};
    CharBuffer inBuf = CharBuffer::allocate(static_cast<size_t>(ulen) + 1);
    inBuf.clear();
    err = U_ZERO_ERROR;
    u_strFromUTF8(reinterpret_cast<UChar*>(inBuf.ptr()), ulen + 1, &ulen, utf8.data(), static_cast<int32_t>(utf8.size()), &err);
    if (U_FAILURE(err)) return {};
    inBuf.position(0).limit(static_cast<size_t>(ulen));
    ByteBuffer outBuf = ByteBuffer::allocate(static_cast<size_t>(ulen) * 4 + 32);
    outBuf.clear();
    encode(inBuf, outBuf);
    outBuf.flip();
    std::vector<uint8_t> result(outBuf.remaining());
    outBuf.get(result.data(), result.size());
    return result;
}

std::vector<uint8_t> CharsetEncoder::encodeChunkCodePoint(int codePoint) {
    if (codePoint < 0) return {};
    std::string utf8;
    if (codePoint < 0x80) {
        utf8.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        utf8.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint < 0x10000) {
        utf8.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    return encodeChunk(utf8);
}

std::vector<uint8_t> CharsetEncoder::encodeFlush() {
    if (!m_cnv) return {};
    ByteBuffer outBuf = ByteBuffer::allocate(64);
    outBuf.clear();
    if (!encodeFlush(outBuf)) return {};
    outBuf.flip();
    std::vector<uint8_t> result(outBuf.remaining());
    outBuf.get(result.data(), result.size());
    return result;
}

bool CharsetEncoder::encode(CharBuffer& in, ByteBuffer& out) {
    if (!m_cnv || !in.hasRemaining()) return false;
    const UChar* source = reinterpret_cast<const UChar*>(in.ptr());
    const UChar* sourceLimit = reinterpret_cast<const UChar*>(in.ptr()) + in.remaining();
    char* target = reinterpret_cast<char*>(out.ptr());
    char* targetLimit = reinterpret_cast<char*>(out.ptr()) + out.remaining();
    UErrorCode err = U_ZERO_ERROR;
    ucnv_fromUnicode(m_cnv, &target, targetLimit, &source, sourceLimit, nullptr, false, &err);
    in.position(static_cast<size_t>(source - reinterpret_cast<const UChar*>(in.data())));
    out.position(static_cast<size_t>(target - reinterpret_cast<char*>(out.data())));
    return in.position() > 0 || out.position() > 0;
}

bool CharsetEncoder::encode(Char32Buffer& in, ByteBuffer& out) {
    if (!m_cnv || !in.hasRemaining() || !out.hasRemaining()) return false;
    size_t inRem = in.remaining();
    size_t u16cap = inRem * 2 + 16;
    CharBuffer u16buf = CharBuffer::allocate(u16cap);
    u16buf.clear();
    int32_t u16len = 0;
    UErrorCode err = U_ZERO_ERROR;
    u_strFromUTF32(reinterpret_cast<UChar*>(u16buf.ptr()), static_cast<int32_t>(u16buf.remaining()), &u16len,
                   reinterpret_cast<const UChar32*>(in.ptr()), static_cast<int32_t>(inRem), &err);
    if (U_FAILURE(err) || u16len <= 0) return false;
    u16buf.position(0).limit(static_cast<size_t>(u16len));
    bool ok = encode(u16buf, out);
    if (ok)
        in.position(in.position() + inRem);
    return ok;
}

bool CharsetEncoder::encodeFlush(ByteBuffer& out) {
    if (!m_cnv) return false;
    const UChar* source = nullptr;
    const UChar* sourceLimit = nullptr;
    char* target = reinterpret_cast<char*>(out.ptr());
    char* targetLimit = reinterpret_cast<char*>(out.ptr()) + out.remaining();
    UErrorCode err = U_ZERO_ERROR;
    ucnv_fromUnicode(m_cnv, &target, targetLimit, &source, sourceLimit, nullptr, true, &err);
    out.position(static_cast<size_t>(target - reinterpret_cast<char*>(out.data())));
    return out.position() > 0;
}

void CharsetEncoder::reset() {
    if (m_cnv) ucnv_resetFromUnicode(m_cnv);
}
