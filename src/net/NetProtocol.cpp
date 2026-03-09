#include "NetProtocol.hpp"

#include "../io/InputStream.hpp"
#include "../io/InputStreamReader.hpp"
#include "../io/PrintStream.hpp"
#include "../io/Reader.hpp"
#include "../io/U32stringReader.hpp"
#include "../io/Uint8ArrayInputStream.hpp"
#include "../io/Writer.hpp"
#include "../util/unicode.hpp"

std::unique_ptr<RandomInputStream> NetProtocol::newRandomInputStream(const URL& url) {
    std::unique_ptr<InputStream> in = newInputStream(url);
    if (!in) return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();
    return std::make_unique<Uint8ArrayInputStream>(std::move(data));
}

std::unique_ptr<Reader> NetProtocol::newReader(const URL& url, std::string_view charset) {
    std::unique_ptr<InputStream> in = newInputStream(url);
    if (!in) return nullptr;
    std::unique_ptr<InputStreamReader> reader = std::make_unique<InputStreamReader>(std::move(in), charset);
    return reader;
}

std::unique_ptr<RandomReader> NetProtocol::newRandomReader(const URL& url, std::string_view charset) {
    // buffer and iconv
    std::unique_ptr<RandomInputStream> in = newRandomInputStream(url);
    if (!in) return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();

    icu::UnicodeString unicode = unicode::fromEncoding(data, charset);
    auto u32 = unicode::convertToU32(unicode);
    return std::make_unique<U32stringReader>(u32);
}

std::unique_ptr<Writer> NetProtocol::newWriter(const URL& url, std::string_view charset) {
    // buffer and iconv
    std::unique_ptr<OutputStream> out = newOutputStream(url, false);
    if (!out) return nullptr;
    return std::make_unique<PrintStream>(std::move(out), charset);
}
