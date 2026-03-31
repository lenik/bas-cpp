#ifndef UTIL_UNICODE_HPP
#define UTIL_UNICODE_HPP

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <bas/locale/unicode.h>

#include <unicode/unistr.h>
#include <unicode/urename.h>

namespace unicode {

/**
 * Decode one UTF-8 code point at [ptr, end).
 * Returns {codepoint, bytes_consumed}. Malformed: {-2, consumed>0}. EOF: {-1, 0}.
 */
inline std::pair<int, int> utf8DecodeCodePoint(const uint8_t* ptr, const uint8_t* end) {
    int consumed = 0;
    int cp = utf8_decode_code_point(ptr, end, &consumed);
    return {cp, consumed};
}

/** 
 * Append UTF-8 encoding of code point \a cp to \a out.
 */
inline void utf8EncodeCodePoint(int cp, std::string& out) {
    uint8_t buf[4];
    int n = 0;
    if (!utf8_encode_code_point(cp, buf, &n) || n <= 0) return;
    out.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
}

/**
 * Append UTF-8 encoding of code point \a cp to \a out.
 */
inline void utf8EncodeCodePoint(int cp, std::vector<uint8_t>& out) {
    uint8_t buf[4];
    int n = 0;
    if (!utf8_encode_code_point(cp, buf, &n) || n <= 0) return;
    out.insert(out.end(), buf, buf + n);
}

inline std::string convertToUTF8(const icu::UnicodeString& ustr) {
    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    char* s = u16_to_utf8(ustr.getBuffer(), ustr.length(), &out_len, &st);
    if (U_FAILURE(st) || s == nullptr) return {};
    std::string out(s, static_cast<size_t>(out_len));
    std::free(s);
    return out;
}

inline std::u32string convertToU32(const icu::UnicodeString& ustr) {
    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    UChar32* buf = u16_to_utf32(ustr.getBuffer(), ustr.length(), &out_len, &st);
    if (U_FAILURE(st) || buf == nullptr || out_len <= 0) {
        if (buf) std::free(buf);
        return {};
    }
    std::u32string out;
    out.resize(static_cast<size_t>(out_len));
    for (int32_t i = 0; i < out_len; ++i)
        out[static_cast<size_t>(i)] = static_cast<char32_t>(buf[i]);
    std::free(buf);
    return out;
}

inline icu::UnicodeString fromEncoding(const char* data, size_t len,
    std::string_view charset, 
    unicode_conversion_mode convertion = UNICODE_ERROR, char32_t replacement = U'\uFFFD', 
    UErrorCode* errorCode = nullptr)
{
    assert(data);

    if (charset.empty()) {
        if (errorCode) *errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return icu::UnicodeString();
    }
    if (len == 0) {
        if (errorCode) *errorCode = U_ZERO_ERROR;
        return icu::UnicodeString();
    }

    UErrorCode status = U_ZERO_ERROR;
    unicode_conversion_mode mode = UNICODE_ERROR;
    switch (convertion) {
        case UNICODE_SKIP: mode = UNICODE_SKIP; break;
        case UNICODE_REPLACE: mode = UNICODE_REPLACE; break;
        case UNICODE_ERROR:
        default: mode = UNICODE_ERROR; break;
    }

    u16_string u16 = u16_from_encoding(
        data,
        static_cast<int32_t>(len),
        std::string(charset).c_str(),
        mode,
        static_cast<UChar32>(replacement),
        &status
    );

    if (errorCode) *errorCode = status;
    if (U_FAILURE(status) || u16.data == nullptr) {
        u16_free(&u16);
        return icu::UnicodeString();
    }

    icu::UnicodeString result(u16.data, u16.length);
    u16_free(&u16);
    return result;
}

inline icu::UnicodeString fromEncoding(const std::vector<uint8_t>& data, std::string_view charset, 
        unicode_conversion_mode convertion = UNICODE_ERROR, char32_t replacement = U'\uFFFD', 
        UErrorCode* errorCode = nullptr) {
    return fromEncoding(reinterpret_cast<const char*>(data.data()), data.size(), charset, convertion, replacement, errorCode);
}

inline icu::UnicodeString fromEncoding(std::string_view data, std::string_view charset, 
        unicode_conversion_mode convertion = UNICODE_ERROR, char32_t replacement = U'\uFFFD', 
        UErrorCode* errorCode = nullptr) {
    return fromEncoding(data.data(), data.size(), charset, convertion, replacement, errorCode);
}

inline std::vector<uint8_t> toEncoding(const icu::UnicodeString& ustr, std::string_view encoding) {
    if (encoding.empty()) return {};

    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    std::string enc(encoding);
    uint8_t* buf = u16_to_encoding(ustr.getBuffer(), ustr.length(), enc.c_str(), &out_len, &st);
    if (U_FAILURE(st) || buf == nullptr || out_len <= 0) {
        if (buf) std::free(buf);
        return {};
    }
    std::vector<uint8_t> out(buf, buf + out_len);
    std::free(buf);
    return out;
}

} // namespace unicode

#endif // UTIL_UNICODE_HPP
