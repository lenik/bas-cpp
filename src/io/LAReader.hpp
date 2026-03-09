#ifndef LAREADER_H
#define LAREADER_H

#include "Reader.hpp"

#include <cstdint>
#include <ios>
#include <string>
#include <vector>

class LAReader : public Reader {
public:
    explicit LAReader(size_t lookAheadCapacity = 1);
    ~LAReader();

    int readChar() override;

    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;

    std::string readLine() override;
    std::string readLineChopped() override;

    int64_t skipChars(int64_t len) override;

    /** Next code point without consuming, or -1 for EOF / negative for error. */
    int lookAheadChar();
    size_t lookAheadChars(char32_t* buf, size_t off, size_t len);

    bool rejectChars(int numCodePoints);

protected:
    virtual int readCharUnbuffered() = 0;

    virtual size_t readCharsUnbuffered(char* buf, size_t off, size_t len) = 0;
    
    virtual size_t skipCharsUnbuffered(size_t numCodePoints) = 0;

    // Get current position in underlying stream
    virtual int64_t charPositionUnbuffered() const = 0;

    virtual bool seekChars(int64_t offset, std::ios::seekdir dir = std::ios::beg) = 0;
    
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

    std::vector<char32_t> m_buffer;  // code points
    size_t m_pos;
    size_t m_limit;
};

#endif // LAREADER_H