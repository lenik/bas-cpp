#ifndef CHARSETDECODER_H
#define CHARSETDECODER_H

#include "ByteBuffer.hpp"
#include "Char32Buffer.hpp"
#include "CharBuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct UConverter;

/**
 * Decodes a byte stream in a given charset to Unicode (UTF-8).
 * Implemented with ICU UConverter.
 */
class CharsetDecoder {
public:
    explicit CharsetDecoder(std::string_view charset);
    ~CharsetDecoder();

    CharsetDecoder(CharsetDecoder&&) noexcept;
    CharsetDecoder& operator=(CharsetDecoder&&) noexcept;
    CharsetDecoder(const CharsetDecoder&) = delete;
    CharsetDecoder& operator=(const CharsetDecoder&) = delete;

    /** Decode bytes to UTF-8. Flushes converter state at end. */
    std::string decode(const uint8_t* src, size_t srcLen);
    std::string decode(std::string_view src);

    /** Decode one chunk; does not flush. For streaming: call decodeChunk for each chunk, then decodeFlush once at end. */
    std::string decodeChunk(const uint8_t* src, size_t srcLen);
    /** Flush internal state and return any remaining output. */
    std::string decodeFlush();

    /** Decode bytes from \a in into \a out. Updates positions. Returns true if any progress. */
    bool decode(ByteBuffer& in, CharBuffer& out);
    /** Flush decoder state into \a out. Updates position. */
    bool decodeFlush(CharBuffer& out);

    /** Decode bytes from \a in into \a out (UTF-32 code points). Updates positions. */
    bool decode(ByteBuffer& in, Char32Buffer& out);
    /** Flush decoder state into \a out (UTF-32). Updates position. */
    bool decodeFlush(Char32Buffer& out);

    void reset();

    std::string_view charset() const { return m_charset; }

private:
    std::string m_charset;
    UConverter* m_cnv;
};

#endif // CHARSETDECODER_H
