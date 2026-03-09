#include "InputSource.hpp"

#include "InputStream.hpp"
#include "StringReader.hpp"
#include "U32stringReader.hpp"
#include "Uint8ArrayInputStream.hpp"

#include "../util/unicode.hpp"

DecodingInputSource::DecodingInputSource(std::string_view charset)
    : m_charset(charset)
{
}

std::unique_ptr<Reader> DecodingInputSource::newReader() {
    std::unique_ptr<InputStream> in = newInputStream();
    if (!in) return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();

    // icu convert from m_charset to utf-8
    auto unicode = unicode::fromEncoding(data, m_charset);
    auto utf8 = unicode::convertToUTF8(unicode);
    return std::make_unique<StringReader>(utf8);
}

std::unique_ptr<RandomReader> DecodingInputSource::newRandomReader() {
    std::unique_ptr<InputStream> in = newInputStream();
    if (!in) return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();

    // icu convert from m_charset to utf-8
    auto unicode = unicode::fromEncoding(data, m_charset);
    auto u32 = unicode::convertToU32(unicode);
    return std::make_unique<U32stringReader>(std::move(u32));
}

EncodingInputSource::EncodingInputSource(std::string_view charset)
    : m_charset(charset)
{
}

std::unique_ptr<InputStream> EncodingInputSource::newInputStream() {
    return newRandomInputStream();
}

std::unique_ptr<RandomInputStream> EncodingInputSource::newRandomInputStream() {
    std::unique_ptr<Reader> reader = newReader();
    if (!reader) return nullptr;
    std::string text = reader->readCharsUntilEOF();
    reader->close();
    auto unicode = icu::UnicodeString::fromUTF8(text);
    std::vector<uint8_t> data = unicode::toEncoding(unicode, m_charset);
    return std::make_unique<Uint8ArrayInputStream>(std::move(data));
}