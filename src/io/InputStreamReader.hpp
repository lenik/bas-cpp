#ifndef INPUTSTREAMREADER_H
#define INPUTSTREAMREADER_H

#include "ByteBuffer.hpp"
#include "CharBuffer.hpp"
#include "CharsetDecoder.hpp"
#include "InputStream.hpp"
#include "Reader.hpp"

#include <memory>
#include <string>
#include <string_view>

/**
 * Reader that decodes bytes from an InputStream using a named charset.
 * Implemented with ICU (CharsetDecoder). Uses ByteBuffer/CharBuffer for decoding.
 */
class InputStreamReader : public Reader {
public:
    InputStreamReader(std::unique_ptr<InputStream> in, std::string_view charset = "UTF-8");
    ~InputStreamReader();

    int readChar() override;
    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    std::string readLineChopped() override;
    int64_t skipChars(int64_t numCodePoints) override;
    void close() override;

    std::string_view charset() const { return m_charset; }

private:
    bool fillDecodedBuffer();

    std::unique_ptr<InputStream> m_in;
    std::string m_charset;
    CharsetDecoder m_decoder;
    ByteBuffer m_inBuf;
    CharBuffer m_charBuf;
    std::string m_decoded;
    size_t m_decodedPos;
    bool m_closed;
    bool m_eof;

    static constexpr size_t READ_BUF_SIZE = 4096;
    static constexpr size_t CHAR_BUF_SIZE = 2048;
};

#endif // INPUTSTREAMREADER_H
