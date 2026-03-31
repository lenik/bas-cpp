#ifndef CHARSETENCODER_H
#define CHARSETENCODER_H

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
 * Encodes Unicode (UTF-8 or code points) to a byte stream in a given charset.
 * Implemented with ICU UConverter.
 */
class CharsetEncoder {
public:
    explicit CharsetEncoder(std::string_view charset);
    ~CharsetEncoder();

    CharsetEncoder(CharsetEncoder&&) noexcept;
    CharsetEncoder& operator=(CharsetEncoder&&) noexcept;
    CharsetEncoder(const CharsetEncoder&) = delete;
    CharsetEncoder& operator=(const CharsetEncoder&) = delete;

    /** Encode UTF-8 string to bytes. Flushes converter state at end. */
    std::vector<uint8_t> encode(std::string_view utf8);
    /** Encode one Unicode code point to bytes. */
    std::vector<uint8_t> encodeCodePoint(int codePoint);

    /** Encode one chunk; does not flush. For streaming: call encodeChunk for each chunk, then encodeFlush once at end. */
    std::vector<uint8_t> encodeChunk(std::string_view utf8);
    /** Encode one code point as chunk (no flush). */
    std::vector<uint8_t> encodeChunkCodePoint(int codePoint);
    /** Flush internal state and return any remaining output. */
    std::vector<uint8_t> encodeFlush();

    /** Encode chars from \a in into \a out. Updates positions. Returns true if any progress. */
    bool encode(CharBuffer& in, ByteBuffer& out);
    
    /** Encode code points from \a in into \a out. Updates positions. */
    bool encode(Char32Buffer& in, ByteBuffer& out);
    
    /** Flush encoder state into \a out. Updates position. */
    bool encodeFlush(ByteBuffer& out);

    void reset();

    std::string_view charset() const { return m_charset; }

private:
    std::string m_charset;
    UConverter* m_cnv;
};

#endif // CHARSETENCODER_H
