#include "StringReadStream.hpp"

#include "DecodingReadStream.hpp"
#include "Uint8ArrayInputStream.hpp"

#include <memory>
#include <vector>

StringReadStream::StringReadStream(const std::string &data, std::string_view charset)
    : m_inner(std::make_unique<DecodingReadStream>(
          std::make_unique<Uint8ArrayInputStream>(std::vector<uint8_t>(data.begin(), data.end())),
          charset.empty() ? "UTF-8" : charset)) {}

int StringReadStream::read() { return m_inner->read(); }
size_t StringReadStream::read(uint8_t *buf, size_t off, size_t len) {
    return m_inner->read(buf, off, len);
}
std::vector<uint8_t> StringReadStream::readBytes(size_t len) { return m_inner->readBytes(len); }
std::vector<uint8_t> StringReadStream::readBytesUntilEOF() { return m_inner->readBytesUntilEOF(); }
std::vector<uint8_t> StringReadStream::readRawLine() { return m_inner->readRawLine(); }
std::vector<uint8_t> StringReadStream::readRawLineChopped() {
    return m_inner->readRawLineChopped();
}
int64_t StringReadStream::skip(int64_t len) { return m_inner->skip(len); }

std::string StringReadStream::getEncoding() const { return m_inner->getEncoding(); }
IReadStream::MalformAction StringReadStream::malformAction() const {
    return m_inner->malformAction();
}
void StringReadStream::setMalformAction(MalformAction action) { m_inner->setMalformAction(action); }
int StringReadStream::malformReplacement() const { return m_inner->malformReplacement(); }
void StringReadStream::setMalformReplacement(int codePoint) {
    m_inner->setMalformReplacement(codePoint);
}
std::vector<uint8_t> StringReadStream::getLastMalformedBytes() const {
    return m_inner->getLastMalformedBytes();
}

int StringReadStream::readChar() { return m_inner->readChar(); }

void StringReadStream::close() { m_inner->close(); }
