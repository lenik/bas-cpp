#include "U32stringSource.hpp"

#include "U32stringReader.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <memory>
#include <string>

namespace {

std::u32string utf8_to_u32(const std::string& utf8) {
    std::u32string out;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(utf8.data());
    const uint8_t* end = p + utf8.size();
    while (p < end) {
        int cp = -1;
        int n = 0;
        if (p + 1 <= end && (p[0] & 0x80) == 0) {
            cp = p[0];
            n = 1;
        } else if (p + 2 <= end && (p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            n = 2;
        } else if (p + 3 <= end && (p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            n = 3;
        } else if (p + 4 <= end && (p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            n = 4;
        }
        if (n == 0 || cp < 0) break;
        out.push_back(static_cast<char32_t>(cp));
        p += n;
    }
    return out;
}

} // namespace

U32stringSource::U32stringSource(const std::u32string& data, std::string_view charset)
    : EncodingInputSource(charset.empty() ? "UTF-8" : charset)
    , m_data(data)
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

U32stringSource::U32stringSource(const std::string& data, std::string_view charset)
    : EncodingInputSource(charset.empty() ? "UTF-8" : charset)
    , m_data(utf8_to_u32(data))
    , m_charset(charset.empty() ? "UTF-8" : charset)
{
}

std::unique_ptr<Reader> U32stringSource::newReader() {
    return std::make_unique<U32stringReader>(m_data);
}

std::unique_ptr<RandomReader> U32stringSource::newRandomReader() {
    return std::make_unique<U32stringReader>(m_data);
}
