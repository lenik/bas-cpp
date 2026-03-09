#ifndef POSITION_H
#define POSITION_H

#include <cstdint>
#include <ios>

class IPosition {
public:
    virtual int64_t position() const = 0;
};

class ISeekable : public IPosition {
public:
    ~ISeekable() {}
    virtual bool canSeek(std::ios::seekdir dir = std::ios::beg) const = 0;
    virtual bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) = 0;
};

class ICharPosition {
public:
    ~ICharPosition() {}
    virtual int64_t charPosition() const = 0;

};

class ICharSeekable : public ICharPosition {
public:
    virtual ~ICharSeekable() {}
    virtual bool canSeekChars(std::ios::seekdir dir = std::ios::beg) const = 0;
    virtual bool seekChars(int64_t offset, std::ios::seekdir dir = std::ios::beg) = 0;
};

class ITextPosition {
public:
    virtual ~ITextPosition() {}
    virtual int line() const = 0;
    virtual int column() const = 0;
};

#endif // POSITION_H