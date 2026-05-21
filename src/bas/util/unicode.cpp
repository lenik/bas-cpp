#include "unicode.hpp"

#include <bas/locale/unicode.h>

#include <unicode/ucnv.h>
#include <unicode/urename.h>

#include <algorithm>

namespace unicode {

std::string convertToUTF8(const icu::UnicodeString& ustr) {
    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    char* s = u16_to_utf8(ustr.getBuffer(), ustr.length(), &out_len, &st);
    if (U_FAILURE(st) || s == nullptr)
        return {};
    std::string out(s, static_cast<size_t>(out_len));
    std::free(s);
    return out;
}

std::u32string convertToU32(const icu::UnicodeString& ustr) {
    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    UChar32* buf = u16_to_utf32(ustr.getBuffer(), ustr.length(), &out_len, &st);
    if (U_FAILURE(st) || buf == nullptr || out_len <= 0) {
        if (buf)
            std::free(buf);
        return {};
    }
    std::u32string out;
    out.resize(static_cast<size_t>(out_len));
    for (int32_t i = 0; i < out_len; ++i)
        out[static_cast<size_t>(i)] = static_cast<char32_t>(buf[i]);
    std::free(buf);
    return out;
}

icu::UnicodeString fromEncoding(const char* data, size_t len, std::string_view charset,
                                unicode_conversion_mode convertion, char32_t replacement,
                                UErrorCode* errorCode) {
    assert(data);

    if (charset.empty()) {
        if (errorCode)
            *errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return icu::UnicodeString();
    }
    if (len == 0) {
        if (errorCode)
            *errorCode = U_ZERO_ERROR;
        return icu::UnicodeString();
    }

    UErrorCode status = U_ZERO_ERROR;
    unicode_conversion_mode mode = UNICODE_ERROR;
    switch (convertion) {
    case UNICODE_SKIP:
        mode = UNICODE_SKIP;
        break;
    case UNICODE_REPLACE:
        mode = UNICODE_REPLACE;
        break;
    case UNICODE_ERROR:
    default:
        mode = UNICODE_ERROR;
        break;
    }

    u16_string u16 =
        u16_from_encoding(data, static_cast<int32_t>(len), std::string(charset).c_str(), mode,
                          static_cast<UChar32>(replacement), &status);

    if (errorCode)
        *errorCode = status;
    if (U_FAILURE(status) || u16.data == nullptr) {
        u16_free(&u16);
        return icu::UnicodeString();
    }

    icu::UnicodeString result(u16.data, u16.length);
    u16_free(&u16);
    return result;
}

std::vector<uint8_t> toEncoding(const icu::UnicodeString& ustr, std::string_view encoding) {
    if (encoding.empty())
        return {};

    UErrorCode st = U_ZERO_ERROR;
    int32_t out_len = 0;
    std::string enc(encoding);
    uint8_t* buf = u16_to_encoding(ustr.getBuffer(), ustr.length(), enc.c_str(), &out_len, &st);
    if (U_FAILURE(st) || buf == nullptr || out_len <= 0) {
        if (buf)
            std::free(buf);
        return {};
    }
    std::vector<uint8_t> out(buf, buf + out_len);
    std::free(buf);
    return out;
}

std::vector<uint8_t> toUtf8(const std::vector<uint8_t>& data, std::string_view charset,
                            unicode_conversion_mode convertion, char32_t replacement,
                            UErrorCode* errorCode) {
    std::string cset(charset);
    std::transform(cset.begin(), cset.end(), cset.begin(), ::tolower);
    if (cset == "utf-8" || cset == "utf8" || cset.empty()) {
        return data;
    } else {
        auto unicode = fromEncoding(data, cset, convertion, replacement, errorCode);
        auto utf8 = toEncoding(unicode, "UTF-8");
        return utf8;
    }
}

std::string toUtf8String(const std::vector<uint8_t>& data, std::string_view charset,
                         unicode_conversion_mode convertion, char32_t replacement,
                         UErrorCode* errorCode) {
    return toUtf8String(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()),
                        charset, convertion, replacement, errorCode);
}

std::string toUtf8String(std::string_view data, std::string_view charset,
                         unicode_conversion_mode convertion, char32_t replacement,
                         UErrorCode* errorCode) {
    std::string cset(charset);
    std::transform(cset.begin(), cset.end(), cset.begin(), ::tolower);
    if (cset == "utf-8" || cset == "utf8" || cset.empty()) {
        return std::string(data);
    } else {
        auto unicode = fromEncoding(data, cset, convertion, replacement, errorCode);
        auto utf8Bytes = toEncoding(unicode, "UTF-8");
        const char* utf8Chars = reinterpret_cast<const char*>(utf8Bytes.data());
        auto utf8 = std::string(utf8Chars, utf8Bytes.size());
        return utf8;
    }
}

} // namespace unicode