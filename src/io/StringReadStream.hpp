#ifndef STRINGREADER_H
#define STRINGREADER_H

#include "ReadStream.hpp"

#include <memory>
#include <string>

/** IReadStream that reads from a string (decoded with charset). Owns a copy of the string bytes. */
class StringReadStream : public IReadStream {
public:
    explicit StringReadStream(const std::string& data, std::string_view charset = "UTF-8");

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    std::vector<uint8_t> readBytes(size_t len) override;
    std::vector<uint8_t> readBytesUntilEOF() override;
    std::vector<uint8_t> readRawLine() override;
    std::vector<uint8_t> readRawLineChopped() override;
    int64_t skip(int64_t len) override;

    std::string getEncoding() const override;
    MalformAction malformAction() const override;
    void setMalformAction(MalformAction action) override;
    int malformReplacement() const override;
    void setMalformReplacement(int codePoint) override;
    std::vector<uint8_t> getLastMalformedBytes() const override;

    int readChar() override;

    void close() override;

private:
    std::unique_ptr<IReadStream> m_inner;
};

#endif // STRINGREADER_H
