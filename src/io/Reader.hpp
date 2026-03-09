#ifndef READER_H
#define READER_H

#include "Position.hpp"

#include <cstdint>
#include <string>

class Reader {
public:
    virtual ~Reader() = default;

    // Read one Unicode code point. Returns code point, -1 for EOF, other negative values for error.
    virtual int readChar() = 0;

    // UTF-8 encoded result (code points)
    virtual std::string readChars(size_t numCodePoints);
    virtual std::string readCharsUntilEOF();

    // UTF-8 encoded; includes EOL / trims trailing EOL
    virtual std::string readLine();
    virtual std::string readLineChopped();

    virtual int64_t skipChars(int64_t numCodePoints);

    virtual void close() = 0;
};

class RandomReader : public Reader, virtual public ICharSeekable {
public:
    ~RandomReader() override = default;

    bool canSeekChars(std::ios::seekdir dir = std::ios::beg) const override {
        return true;
    }
};

#endif // READER_H