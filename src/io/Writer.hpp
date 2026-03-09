#ifndef WRITER_H
#define WRITER_H

#include <string_view>

class Writer {
public:
    ~Writer() = default;

    // Write one Unicode code point (as int). Returns true on success, false on failure.
    virtual bool writeChar(char32_t codePoint) = 0;

    // UTF-8 encoded string (code points)
    virtual bool write(std::string_view data);
    virtual bool writeln();
    virtual bool writeln(std::string_view data);

    virtual void flush();

    virtual void close();
};

#endif // WRITER_H