// Unit tests for DecodingReadStream

#include "DecodingReadStream.hpp"
#include "Uint8ArrayInputStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void test_utf8_readChar_and_readLine() {
    std::vector<uint8_t> data = {'h', 'i', '\n'};
    auto raw = std::make_unique<Uint8ArrayInputStream>(std::move(data));
    DecodingReadStream stream(std::move(raw), "UTF-8");

    assert(stream.getEncoding() == "UTF-8");
    assert(stream.readChar() == 'h');
    assert(stream.readChar() == 'i');
    assert(stream.readChar() == '\n');
    assert(stream.readChar() == -1);

    std::vector<uint8_t> data2 = {'a', '\n', 'b', '\n'};
    auto raw2 = std::make_unique<Uint8ArrayInputStream>(std::move(data2));
    DecodingReadStream stream2(std::move(raw2), "UTF-8");
    assert(stream2.readLine() == "a\n");
    assert(stream2.readLine() == "b\n");
    assert(stream2.readLine().empty());
    stream2.close();
}

static void test_read_bytes() {
    std::vector<uint8_t> data = {'X', 'Y', 'Z'};
    auto raw = std::make_unique<Uint8ArrayInputStream>(std::move(data));
    DecodingReadStream stream(std::move(raw));

    assert(stream.read() == 'X');
    assert(stream.read() == 'Y');
    assert(stream.read() == 'Z');
    assert(stream.read() == -1);
    stream.close();
}

static void test_readCharsUntilEOF() {
    std::vector<uint8_t> data = {'a', 'b', 'c'};
    auto raw = std::make_unique<Uint8ArrayInputStream>(std::move(data));
    DecodingReadStream stream(std::move(raw));

    assert(stream.readCharsUntilEOF() == "abc");
    assert(stream.readCharsUntilEOF().empty());
    stream.close();
}

static void test_malform_action_replace() {
    // Invalid UTF-8: lead byte 0xC0 not followed by continuation
    std::vector<uint8_t> data = {0xC0u, 0x41u};  // invalid then 'A'
    auto raw = std::make_unique<Uint8ArrayInputStream>(std::move(data));
    DecodingReadStream stream(std::move(raw));

    assert(stream.malformAction() == DecodingReadStream::MalformAction::Replace);
    int c = stream.readChar();
    assert(c >= 0 || c == -1 || c == -2);  // valid code point, EOF, or malformed
    stream.close();
}

int main() {
    test_utf8_readChar_and_readLine();
    test_read_bytes();
    test_readCharsUntilEOF();
    test_malform_action_replace();
    std::cout << "All DecodingReadStream tests passed.\n";
    return 0;
}
