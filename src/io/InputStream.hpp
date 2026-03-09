#ifndef INPUTSTREAM_H
#define INPUTSTREAM_H

#include "Position.hpp"

#include <cstdint>
#include <vector>

class InputStream {
public:
    virtual ~InputStream() {}
    
    // Read single byte. Returns [0-255] or -1 for EOF.
    virtual int read() = 0;

    // Read up to len bytes into buf+off. Returns bytes read or 0 on EOF.
    virtual size_t read(uint8_t* buf, size_t off, size_t len);

    // Bulk helpers
    virtual std::vector<uint8_t> readBytes(size_t len);
    virtual std::vector<uint8_t> readBytesUntilEOF();
    virtual std::vector<uint8_t> readRawLine();     // includes EOL
    virtual std::vector<uint8_t> readRawLineChopped();  // trims trailing EOL
    virtual int64_t skip(int64_t len);

    virtual void close() = 0;
};

class RandomInputStream : public InputStream, virtual public ISeekable {
public:
    bool canSeek(std::ios::seekdir dir = std::ios::beg) const override {
        return true;
    }

};

#endif // INPUTSTREAM_H