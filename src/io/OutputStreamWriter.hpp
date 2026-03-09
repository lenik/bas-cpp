#ifndef OUTPUTSTREAMWRITER_H
#define OUTPUTSTREAMWRITER_H

#include "ByteBuffer.hpp"
#include "CharBuffer.hpp"
#include "CharsetEncoder.hpp"
#include "OutputStream.hpp"
#include "Writer.hpp"

#include <memory>
#include <string_view>

/**
 * Writer that encodes characters to an OutputStream using a named charset.
 * Implemented with ICU (CharsetEncoder). Uses ByteBuffer for encoded output.
 */
class OutputStreamWriter : public Writer {
public:
    OutputStreamWriter(std::unique_ptr<OutputStream> out, std::string_view charset = "UTF-8");
    ~OutputStreamWriter();

    bool writeChar(char32_t codePoint) override;
    bool write(std::string_view data) override;
    bool writeln() override;
    bool writeln(std::string_view data) override;
    void flush() override;
    void close() override;

    std::string_view charset() const { return m_charset; }

private:
    void flushOutBuf();

    std::unique_ptr<OutputStream> m_out;
    std::string m_charset;
    CharsetEncoder m_encoder;
    ByteBuffer m_outBuf;
    bool m_closed;

    static constexpr size_t OUT_BUF_SIZE = 4096;
};

#endif // OUTPUTSTREAMWRITER_H
