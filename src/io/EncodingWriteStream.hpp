#ifndef ENCODINGWRITESTREAM_H
#define ENCODINGWRITESTREAM_H

#include "WriteStream.hpp"

#include <string>

class EncodingWriteStream : public IWriteStream {
public:
    explicit EncodingWriteStream(std::string_view encoding = "UTF-8");
    ~EncodingWriteStream() override = default;

    std::string getEncoding() const override { return m_encoding; }
    MalformAction malformAction() const override { return m_malformAction; }
    void setMalformAction(MalformAction action) override { m_malformAction = action; }
    int malformReplacement() const override { return m_malformReplacement; }
    void setMalformReplacement(int codePoint) override { m_malformReplacement = codePoint; }

    bool write(int byte) override;
    bool writeChar(char32_t codePoint) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    bool write(std::string_view data) override;
    bool writeln(std::string_view data) override;
    void flush() override = 0;
    void close() override = 0;

protected:
    // Write raw bytes to the underlying sink. Return bytes written, -1 on error.
    virtual size_t _write(const uint8_t* buf, size_t len) = 0;

private:
    std::string m_encoding;
    MalformAction m_malformAction;
    int m_malformReplacement;
};

#endif // ENCODINGWRITESTREAM_H