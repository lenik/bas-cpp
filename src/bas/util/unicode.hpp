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
    if (!utf8_encode_code_point(cp, buf, &n) || n <= 0)
        return;
    out.append(reinterpret_cast<const char*>(buf), static_cast<size_t>(n));
}

/**
 * Append UTF-8 encoding of code point \a cp to \a out.
 */
inline void utf8EncodeCodePoint(int cp, std::vector<uint8_t>& out) {
    uint8_t buf[4];
    int n = 0;
    if (!utf8_encode_code_point(cp, buf, &n) || n <= 0)
        return;
    out.insert(out.end(), buf, buf + n);
}

std::string convertToUTF8(const icu::UnicodeString& ustr);

std::u32string convertToU32(const icu::UnicodeString& ustr);

icu::UnicodeString fromEncoding(const char* data, size_t len, std::string_view charset,
                                unicode_conversion_mode convertion = UNICODE_ERROR,
                                char32_t replacement = U'\uFFFD', UErrorCode* errorCode = nullptr);

inline icu::UnicodeString fromEncoding(const std::vector<uint8_t>& data, std::string_view charset,
                                       unicode_conversion_mode convertion = UNICODE_ERROR,
                                       char32_t replacement = U'\uFFFD',
                                       UErrorCode* errorCode = nullptr) {
    return fromEncoding(reinterpret_cast<const char*>(data.data()), data.size(), charset,
                        convertion, replacement, errorCode);
}

inline icu::UnicodeString fromEncoding(std::string_view data, std::string_view charset,
                                       unicode_conversion_mode convertion = UNICODE_ERROR,
                                       char32_t replacement = U'\uFFFD',
                                       UErrorCode* errorCode = nullptr) {
    return fromEncoding(data.data(), data.size(), charset, convertion, replacement, errorCode);
}

std::vector<uint8_t> toEncoding(const icu::UnicodeString& ustr, std::string_view encoding);

std::vector<uint8_t> toUtf8(const std::vector<uint8_t>& data, std::string_view charset,
                            unicode_conversion_mode convertion = UNICODE_ERROR,
                            char32_t replacement = U'\uFFFD', UErrorCode* errorCode = nullptr);

std::string toUtf8String(const std::vector<uint8_t>& data, std::string_view charset,
                         unicode_conversion_mode convertion = UNICODE_ERROR,
                         char32_t replacement = U'\uFFFD', UErrorCode* errorCode = nullptr);

std::string toUtf8String(std::string_view data, std::string_view charset,
                         unicode_conversion_mode convertion = UNICODE_ERROR,
                         char32_t replacement = U'\uFFFD', UErrorCode* errorCode = nullptr);

} // namespace unicode

#endif // UTIL_UNICODE_HPP
