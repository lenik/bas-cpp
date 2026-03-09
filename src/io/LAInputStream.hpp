#ifndef LAINPUTSTREAM_H
#define LAINPUTSTREAM_H

#include "InputStream.hpp"
#include "Position.hpp"

#include <cstdint>
#include <ios>
#include <vector>

/**
 * LAInputStream is a stream that allows for look ahead and reject functionality.
 */
class LAInputStream : public InputStream, virtual public ISeekable {
public:
    LAInputStream(size_t lookAheadCapacity = 1);

    int read() override; // implemented via read(buffer)
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    std::vector<uint8_t> readBytes(size_t len) override;
    std::vector<uint8_t> readBytesUntilEOF() override;
    std::vector<uint8_t> readRawLine() override;
    std::vector<uint8_t> readRawLineChopped() override;
    
    int64_t position() const override;

    bool canSeek(std::ios::seekdir dir = std::ios::beg) const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;
    
    int64_t skip(int64_t len) override;

    int lookAhead();
    size_t lookAhead(uint8_t* buf, size_t off, size_t len);

    // Reject (seek back) len bytes. Returns true if successful.
    bool reject(int len);

    void discardPrefetch();

protected:

    // Read a single raw byte from internal buffer. Return [0-255] or -1 for EOF.
    virtual int readUnbuffered() = 0;

    // Read from underlying source directly. Returns actual byte count, 0 on EOF or error.
    virtual size_t readUnbuffered(uint8_t* buf, size_t off,size_t len) = 0;
    
    virtual size_t skipUnbuffered(size_t len) = 0;
    
    // Get current position in underlying stream
    virtual int64_t positionUnbuffered() const = 0;
    
    // Seek in underlying stream (for reject functionality)
    virtual bool seekUnbuffered(int64_t offset, std::ios::seekdir dir = std::ios::beg) = 0;
    
private:
    // copies the remaining unread bytes to the beginning of the buffer
    void compact();

    // Prefetch the given length of bytes into the buffer.
    // limit += len
    // return true if exactly len bytes were read and added to the buffer.
    bool prefetch(size_t len);

    // try to expand the buffer to at least the given size
    bool expandCapacity(size_t capacity);

    size_t m_lookAheadCapacity;
    bool m_unlimit;

    std::vector<uint8_t> m_buffer;
    size_t m_pos;       // Current position in the buffer
    size_t m_limit;     // Valid data up to this position

    int64_t m_position;
};

#endif // LAINPUTSTREAM_H