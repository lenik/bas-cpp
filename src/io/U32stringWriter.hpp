#ifndef U32STRINGWRITER_H
#define U32STRINGWRITER_H

#include "Writer.hpp"

#include <string>

/** Writer into an internal UTF-32 string buffer. Use str() to get the result. */
class U32stringWriter : public Writer {
public:
    ~U32stringWriter() = default;

    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    std::u32string str() const { return m_buffer; }

private:
    std::u32string m_buffer;
};

#endif // U32STRINGWRITER_H
