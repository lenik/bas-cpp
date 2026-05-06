#include "ByteBuffer.hpp"
#include "Char32Buffer.hpp"
#include "CharBuffer.hpp"
#include "CharsetEncoder.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "FAIL: " << #a << " == " << #b << " (" << (a) << " != " << (b) << ")\n"; return 1; } } while(0)
#define ASSERT_TRUE(c) do { if (!(c)) { std::cerr << "FAIL: " << #c << "\n"; return 1; } } while(0)

static int test_utf8_encode() {
    CharsetEncoder enc("UTF-8");
    ASSERT_EQ(enc.charset(), "UTF-8");
    std::vector<uint8_t> out = enc.encode(std::string_view("hello"));
    ASSERT_EQ(out.size(), 5u);
    ASSERT_EQ(std::string(out.begin(), out.end()), "hello");
    out = enc.encode(std::string_view(""));
    ASSERT_TRUE(out.empty());
    return 0;
}

static int test_utf8_encode_unicode() {
    CharsetEncoder enc("UTF-8");
    std::vector<uint8_t> out = enc.encode(std::string_view("caf\xc3\xa9"));  // café -> 5 bytes
    ASSERT_EQ(out.size(), 5u);
    ASSERT_EQ(static_cast<unsigned char>(out[3]), 0xc3u);
    ASSERT_EQ(static_cast<unsigned char>(out[4]), 0xa9u);
    return 0;
}

static int test_encode_code_point() {
    CharsetEncoder enc("UTF-8");
    std::vector<uint8_t> out = enc.encodeCodePoint('A');
    ASSERT_EQ(out.size(), 1u);
    ASSERT_EQ(out[0], 0x41u);
    out = enc.encodeCodePoint(0x00E9);
    ASSERT_EQ(out.size(), 2u);
    ASSERT_EQ(static_cast<unsigned char>(out[0]), 0xc3u);
    ASSERT_EQ(static_cast<unsigned char>(out[1]), 0xa9u);
    return 0;
}

static int test_encode_chunk_flush() {
    CharsetEncoder enc("UTF-8");
    std::vector<uint8_t> c1 = enc.encodeChunk(std::string_view("he"));
    std::vector<uint8_t> c2 = enc.encodeChunk(std::string_view("llo"));
    std::vector<uint8_t> f = enc.encodeFlush();
    std::string s;
    for (uint8_t b : c1) s += static_cast<char>(b);
    for (uint8_t b : c2) s += static_cast<char>(b);
    for (uint8_t b : f) s += static_cast<char>(b);
    ASSERT_EQ(s, "hello");
    return 0;
}

static int test_iso8859_1() {
    CharsetEncoder enc("ISO-8859-1");
    ASSERT_EQ(enc.charset(), "ISO-8859-1");
    // UTF-8 é (0xC3 0xA9) -> ISO-8859-1 0xE9
    std::vector<uint8_t> out = enc.encode(std::string_view("\xc3\xa9"));
    ASSERT_EQ(out.size(), 1u);
    ASSERT_EQ(out[0], 0xE9u);
    return 0;
}

static int test_reset() {
    CharsetEncoder enc("UTF-8");
    enc.encode(std::string_view("a"));
    enc.reset();
    std::vector<uint8_t> out = enc.encode(std::string_view("b"));
    ASSERT_EQ(out.size(), 1u);
    ASSERT_EQ(out[0], 'b');
    return 0;
}

static int test_invalid_charset() {
    try {
        CharsetEncoder enc("NoSuchCharsetXYZ");
        std::cerr << "FAIL: expected exception\n";
        return 1;
    } catch (const std::runtime_error&) {
        return 0;
    }
}

static int test_encode_char32_buffer() {
    CharsetEncoder enc("UTF-8");
    Char32Buffer in = Char32Buffer::allocate(8);
    in.clear().put(U'A').put(U'B').flip();
    ByteBuffer out = ByteBuffer::allocate(16);
    out.clear();
    bool ok = enc.encode(in, out);
    ASSERT_TRUE(ok);
    out.flip();
    ASSERT_TRUE(out.remaining() >= 2u);
    ASSERT_EQ(out.get(), 0x41u);
    ASSERT_EQ(out.get(), 0x42u);
    return 0;
}

int main() {
    if (test_utf8_encode()) return 1;
    if (test_utf8_encode_unicode()) return 1;
    if (test_encode_code_point()) return 1;
    if (test_encode_chunk_flush()) return 1;
    if (test_iso8859_1()) return 1;
    if (test_reset()) return 1;
    if (test_invalid_charset()) return 1;
    if (test_encode_char32_buffer()) return 1;
    std::cout << "All CharsetEncoder tests passed.\n";
    return 0;
}
