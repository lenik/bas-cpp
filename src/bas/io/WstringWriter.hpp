#ifndef WSTRINGWRITER_H
#define WSTRINGWRITER_H

#include "Writer.hpp"

#include <string>

/** Writer into an internal wide string buffer. Use str() to get the result. */
class WstringWriter : public Writer {
public:
    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    std::wstring str() const { return m_buffer; }

private:
    std::wstring m_buffer;
};

#endif // WSTRINGWRITER_H
