#ifndef STRINGWRITER_H
#define STRINGWRITER_H

#include "Writer.hpp"

#include <string>

/** Writer into an internal UTF-8 string buffer. Use str() to get the result. */
class StringWriter : public Writer {
public:
    StringWriter() = default;

    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    /** Get the accumulated UTF-8 string. */
    std::string str() const { return m_buffer; }

private:
    std::string m_buffer;
};

#endif // STRINGWRITER_H
