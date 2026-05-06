// Unit tests for InputStreamReader

#include "ConstBufferInputStream.hpp"
#include "InputStreamReader.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

static void test_utf8_readChar() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("AB"));
    InputStreamReader r(std::move(in), "UTF-8");
    assert(r.charset() == "UTF-8");
    assert(r.readChar() == 'A');
    assert(r.readChar() == 'B');
    assert(r.readChar() == -1);
}

static void test_utf8_readChars() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("abc"));
    InputStreamReader r(std::move(in), "UTF-8");
    assert(r.readChars(2) == "ab");
    assert(r.readChars(1) == "c");
    assert(r.readChars(1).empty());
}

static void test_utf8_readCharsUntilEOF() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("xyz"));
    InputStreamReader r(std::move(in), "UTF-8");
    assert(r.readCharsUntilEOF() == "xyz");
    assert(r.readCharsUntilEOF().empty());
}

static void test_utf8_readLine() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("L1\nL2\n"));
    InputStreamReader r(std::move(in), "UTF-8");
    assert(r.readLineChopped() == "L1\n");
    assert(r.readLineChopped() == "L2\n");
    assert(r.readLineChopped().empty());
}

static void test_utf8_skipChars() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("12345"));
    InputStreamReader r(std::move(in), "UTF-8");
    assert(r.skipChars(2) == 2);
    assert(r.readChar() == '3');
}

static void test_close() {
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view("x"));
    InputStreamReader r(std::move(in), "UTF-8");
    r.close();
    assert(r.readChar() == -1);
}

static void test_iso8859_1() {
    // Latin-1: 0xC0 = À, 0xE9 = é
    const char bytes[] = { static_cast<char>(0xC0), static_cast<char>(0xE9) };
    auto in = std::make_unique<ConstBufferInputStream>(std::string_view(bytes, 2));
    InputStreamReader r(std::move(in), "ISO-8859-1");
    std::string s = r.readCharsUntilEOF();
    assert(s.size() == 4u);
    assert(static_cast<unsigned char>(s[0]) == 0xC3u && static_cast<unsigned char>(s[1]) == 0x80u);
    assert(static_cast<unsigned char>(s[2]) == 0xC3u && static_cast<unsigned char>(s[3]) == 0xA9u);
}

static void test_null_stream_throws() {
    try {
        InputStreamReader r(nullptr, "UTF-8");
        std::cerr << "FAIL: expected exception for null stream\n";
        assert(false);
    } catch (const std::invalid_argument&) {
        // expected
    }
}

int main() {
    test_utf8_readChar();
    test_utf8_readChars();
    test_utf8_readCharsUntilEOF();
    test_utf8_readLine();
    test_utf8_skipChars();
    test_close();
    test_iso8859_1();
    test_null_stream_throws();
    std::cout << "All InputStreamReader tests passed.\n";
    return 0;
}
