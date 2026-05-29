#ifndef IWRITESTREAM_H
#define IWRITESTREAM_H

#include "OutputStream.hpp"
#include "Writer.hpp"

#include <cstdint>
#include <string>

class IWriteStream : public OutputStream, public Writer {
public:
    ~IWriteStream() override = default;

    enum class MalformAction {
        Skip,
        Replace,
        Error
    };

    virtual std::string getEncoding() const = 0;
    virtual MalformAction malformAction() const = 0;
    virtual void setMalformAction(MalformAction action) = 0;
    virtual int malformReplacement() const = 0;
    virtual void setMalformReplacement(int codePoint) = 0;

    using Writer::write;
    using OutputStream::write;
    
    // Write a single byte (0-255). Returns true on success.
    // bool write(int byte) override = 0;

    // Write one Unicode code point. Returns true on success, false on encoding failure.
    // bool writeChar(char32_t codePoint) override = 0;

    // Write len bytes from buf+off. Returns bytes written, or 0 on failure.
    // size_t write(const uint8_t* buf, size_t off, size_t len) override = 0;

    // Convenience write for UTF-8 string with current encoding.
    // bool write(std::string_view data) override = 0;
    // bool writeln(std::string_view data) override = 0;

    // void flush() override = 0;
    // void close() override = 0;
    using Writer::flush;
    using Writer::close;
};

#endif // IWRITESTREAM_H

