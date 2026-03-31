#ifndef IWRITESTREAM_H
#define IWRITESTREAM_H

#include "OutputStream.hpp"
#include "Writer.hpp"

#include <cstdint>
#include <string>

class IWriteStream : public OutputStream, public Writer {
public:
    virtual ~IWriteStream() = default;

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

    // Write a single byte (0-255). Returns true on success.
    virtual bool write(int byte) = 0;

    // Write one Unicode code point. Returns true on success, false on encoding failure.
    virtual bool writeChar(char32_t codePoint) = 0;

    // Write len bytes from buf+off. Returns bytes written, or 0 on failure.
    virtual size_t write(const uint8_t* buf, size_t off, size_t len) = 0;

    // Convenience write for UTF-8 string with current encoding.
    virtual bool write(std::string_view data) = 0;
    virtual bool writeln(std::string_view data) = 0;

    virtual void flush() = 0;

    virtual void close() = 0;
};

#endif // IWRITESTREAM_H

