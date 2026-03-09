#ifndef OUTPUTSTREAM_H
#define OUTPUTSTREAM_H

#include <cstddef>
#include <cstdint>
#include <vector>

class OutputStream {
public:
    virtual ~OutputStream() {}

    // Write a single byte (0-255). Returns true on success.
    virtual bool write(int byte) = 0;

    // Write len bytes from buf+off. Returns bytes written, or 0 on failure.
    virtual size_t write(const uint8_t* buf, size_t off, size_t len);
    virtual size_t write(std::vector<uint8_t> data);

    virtual void flush() = 0;

    virtual void close() = 0;
};

#endif // OUTPUTSTREAM_H