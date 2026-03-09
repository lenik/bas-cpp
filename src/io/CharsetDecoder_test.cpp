#include "ByteBuffer.hpp"
#include "Char32Buffer.hpp"
#include "CharBuffer.hpp"
#include "CharsetDecoder.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "FAIL: " << #a << " == " << #b << " (" << (a) << " != " << (b) << ")\n"; return 1; } } while(0)
#define ASSERT_TRUE(c) do { if (!(c)) { std::cerr << "FAIL: " << #c << "\n"; return 1; } } while(0)

static int test_utf8_decode_string_view() {
    CharsetDecoder dec("UTF-8");
    ASSERT_EQ(dec.charset(), "UTF-8");
    std::string out = dec.decode(std::string_view("hello"));
    ASSERT_EQ(out, "hello");
    out = dec.decode(std::string_view(""));
    ASSERT_EQ(out, "");
    // Multi-byte UTF-8: U+00E9 (é), U+2014 (em dash)
    out = dec.decode(std::string_view("caf\xc3\xa9"));
    ASSERT_EQ(out, "caf\xc3\xa9");
    out = dec.decode(std::string_view("\xe2\x80\x94"));
    ASSERT_EQ(out.size(), 3u);
    ASSERT_EQ(static_cast<unsigned char>(out[0]), 0xe2u);
    ASSERT_EQ(static_cast<unsigned char>(out[1]), 0x80u);
    ASSERT_EQ(static_cast<unsigned char>(out[2]), 0x94u);
    return 0;
}

static int test_utf8_decode_ptr_len() {
    CharsetDecoder dec("UTF-8");
    const char* s = "abc";
    std::string out = dec.decode(reinterpret_cast<const uint8_t*>(s), 3);
    ASSERT_EQ(out, "abc");
    out = dec.decode(nullptr, 0);
    ASSERT_EQ(out, "");
    return 0;
}

static int test_decode_chunk_flush() {
    CharsetDecoder dec("UTF-8");
    // Decode in two chunks
    std::string c1 = dec.decodeChunk(reinterpret_cast<const uint8_t*>("he"), 2);
    std::string c2 = dec.decodeChunk(reinterpret_cast<const uint8_t*>("llo"), 3);
    std::string flush = dec.decodeFlush();
    ASSERT_EQ(c1, "he");
    ASSERT_EQ(c2, "llo");
    ASSERT_TRUE(flush.empty() || flush.size() <= 1u);
    ASSERT_EQ(c1 + c2 + flush, "hello");
    return 0;
}

static int test_decode_bytebuffer_charbuffer() {
    CharsetDecoder dec("UTF-8");
    ByteBuffer in = ByteBuffer::allocate(64);
    in.clear().put(reinterpret_cast<const uint8_t*>("Hi"), 2).flip();
    CharBuffer out = CharBuffer::allocate(32);
    out.clear();
    bool ok = dec.decode(in, out);
    ASSERT_TRUE(ok);
    out.flip();
    ASSERT_TRUE(out.remaining() >= 2u);
    ASSERT_EQ(out.get(), 'H');
    ASSERT_EQ(out.get(), 'i');
    return 0;
}

static int test_decode_flush_charbuffer() {
    CharsetDecoder dec("UTF-8");
    ByteBuffer in = ByteBuffer::allocate(16);
    in.clear().put(reinterpret_cast<const uint8_t*>("x"), 1).flip();
    CharBuffer out = CharBuffer::allocate(32);
    out.clear();
    dec.decode(in, out);
    out.flip();
    size_t pos = out.position();
    out.clear().position(pos);
    bool flushed = dec.decodeFlush(out);
    // After full decode, flush may produce 0 or little output
    ASSERT_TRUE(flushed || out.position() == pos);
    return 0;
}

static int test_decode_bytebuffer_char32buffer() {
    CharsetDecoder dec("UTF-8");
    ByteBuffer in = ByteBuffer::allocate(64);
    in.clear().put(reinterpret_cast<const uint8_t*>("A"), 1).flip();
    Char32Buffer out = Char32Buffer::allocate(16);
    out.clear();
    bool ok = dec.decode(in, out);
    ASSERT_TRUE(ok);
    out.flip();
    ASSERT_TRUE(out.remaining() >= 1u);
    ASSERT_EQ(out.get(), U'A');
    return 0;
}

static int test_reset() {
    CharsetDecoder dec("UTF-8");
    dec.decode(std::string_view("a"));
    dec.reset();
    std::string out = dec.decode(std::string_view("b"));
    ASSERT_EQ(out, "b");
    return 0;
}

static int test_invalid_charset() {
    try {
        CharsetDecoder dec("NoSuchCharsetXYZ");
        std::cerr << "FAIL: expected exception for invalid charset\n";
        return 1;
    } catch (const std::runtime_error&) {
        return 0;
    }
}

static int test_iso8859_1() {
    CharsetDecoder dec("ISO-8859-1");
    ASSERT_EQ(dec.charset(), "ISO-8859-1");
    // 0xC0 in ISO-8859-1 is Latin capital letter A with grave (À)
    const uint8_t bytes[] = { 0xC0, 0xE9 };  // À, é
    std::string out = dec.decode(bytes, sizeof bytes);
    ASSERT_EQ(out.size(), 4u);  // UTF-8: \xc3\x80 \xc3\xa9
    ASSERT_EQ(static_cast<unsigned char>(out[0]), 0xc3u);
    ASSERT_EQ(static_cast<unsigned char>(out[1]), 0x80u);
    ASSERT_EQ(static_cast<unsigned char>(out[2]), 0xc3u);
    ASSERT_EQ(static_cast<unsigned char>(out[3]), 0xa9u);
    return 0;
}

int main() {
    if (test_utf8_decode_string_view()) return 1;
    if (test_utf8_decode_ptr_len()) return 1;
    if (test_decode_chunk_flush()) return 1;
    if (test_decode_bytebuffer_charbuffer()) return 1;
    if (test_decode_flush_charbuffer()) return 1;
    if (test_decode_bytebuffer_char32buffer()) return 1;
    if (test_reset()) return 1;
    if (test_invalid_charset()) return 1;
    if (test_iso8859_1()) return 1;
    std::cout << "All CharsetDecoder tests passed.\n";
    return 0;
}
