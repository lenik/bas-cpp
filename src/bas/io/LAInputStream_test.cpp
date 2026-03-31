// Unit tests for LAInputStream interface (tested via LAInputStreamFromStream)

#include "ConstBufferInputStream.hpp"
#include "LAInputStreamFromStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void test_readBytes() {
    std::string data = "hello";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 8);

    std::vector<uint8_t> two = in.readBytes(2);
    assert(two.size() == 2u);
    assert(two[0] == 'h' && two[1] == 'e');
    std::vector<uint8_t> rest = in.readBytes(10);
    assert(rest.size() == 3u);
    assert(rest[0] == 'l' && rest[1] == 'l' && rest[2] == 'o');
    in.close();
}

static void test_readRawLine() {
    std::string data = "line1\nline2\n";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 8);

    std::vector<uint8_t> line1 = in.readRawLine();
    assert(line1.size() == 6u);  // "line1\n"
    assert(line1.back() == '\n');
    std::vector<uint8_t> line2 = in.readRawLine();
    assert(line2.size() == 6u);
    std::vector<uint8_t> empty = in.readRawLine();
    assert(empty.empty());
    in.close();
}

static void test_skip() {
    std::string data = "ABCDEF";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 4);

    assert(in.skip(2) == 2);
    assert(in.read() == 'C');
    assert(in.skip(10) == 3);  // rest is 3 bytes
    assert(in.read() == -1);
    in.close();
}

static void test_readBytesUntilEOF() {
    std::string data = "xyz";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 4);

    std::vector<uint8_t> all = in.readBytesUntilEOF();
    assert(all.size() == 3u);
    assert(all[0] == 'x' && all[1] == 'y' && all[2] == 'z');
    in.close();
}

int main() {
    test_readBytes();
    test_readRawLine();
    test_skip();
    test_readBytesUntilEOF();
    std::cout << "All LAInputStream tests passed.\n";
    return 0;
}
