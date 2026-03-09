#include "EncodingWriteStream.hpp"

#include "../util/unicode.hpp"

#include <string>

EncodingWriteStream::EncodingWriteStream(std::string_view encoding)
    : m_encoding(encoding.empty() ? "UTF-8" : encoding)
    , m_malformAction(MalformAction::Replace)
    , m_malformReplacement('?')
{
}

bool EncodingWriteStream::write(int byte) {
    uint8_t b = static_cast<uint8_t>(byte & 0xFF);
    return _write(&b, 1) == 1;
}

bool EncodingWriteStream::writeChar(char32_t codePoint) {
    std::string encoded;
    unicode::utf8EncodeCodePoint(codePoint, encoded);
    if (encoded.empty()) {
        switch (m_malformAction) {
        case MalformAction::Skip:
            return true;
        case MalformAction::Replace: {
            std::string rep;
            unicode::utf8EncodeCodePoint(m_malformReplacement, rep);
            if (rep.empty()) return false;
            return _write(reinterpret_cast<const uint8_t*>(rep.data()), rep.size()) == rep.size();
        }
        case MalformAction::Error:
        default:
            return false;
        }
    }
    return _write(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size()) == encoded.size();
}

size_t EncodingWriteStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (!buf || len <= 0 || off < 0) return 0;
    return _write(buf + off, len);
}

bool EncodingWriteStream::write(std::string_view data) {
    if (data.empty()) return true;
    // Assume data is UTF-8 already
    return _write(reinterpret_cast<const uint8_t*>(data.data()), data.size()) == data.size();
}

bool EncodingWriteStream::writeln(std::string_view data) {
    if (!write(data))
        return false;
     if (!write('\n'))
        return false;
    return true;
}

