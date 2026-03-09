#ifndef PRINTSTREAM_H
#define PRINTSTREAM_H

#include "OutputStream.hpp"
#include "OutputStreamWriter.hpp"
#include "Writer.hpp"

#include <memory>
#include <string_view>

/**
 * Writer that wraps an OutputStream via OutputStreamWriter.
 * PrintStream(out, charset) is equivalent to OutputStreamWriter(out, charset).
 */
class PrintStream : public Writer {
public:
    PrintStream(std::unique_ptr<OutputStream> out, std::string_view charset = "UTF-8");
    ~PrintStream();

    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    std::string_view charset() const { return m_writer.charset(); }

private:
    OutputStreamWriter m_writer;
};

#endif // PRINTSTREAM_H
