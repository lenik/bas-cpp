#ifndef PRINTWRITER_H
#define PRINTWRITER_H

#include "Writer.hpp"

#include <string_view>

/**
 * Writer that wraps another Writer and provides print/println naming.
 * Delegates all operations to the underlying Writer.
 */
class PrintWriter : public Writer {
public:
    explicit PrintWriter(Writer* out);

    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    /** Alias for write(data). */
    bool print(std::string_view data) { return write(data); }
    /** Alias for writeln(). */
    bool println() { return writeln(); }
    /** Alias for writeln(data). */
    bool println(std::string_view data) { return writeln(data); }

    Writer* underlying() const { return m_out; }

private:
    Writer* m_out;
};

#endif // PRINTWRITER_H
