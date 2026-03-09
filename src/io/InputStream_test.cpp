// Unit tests for InputStream (base class) via ConstBufferInputStream

#include "ConstBufferInputStream.hpp"

#include <cassert>
#include <iostream>
#include <string_view>
#include <vector>

static void test_readBytes() {
    ConstBufferInputStream in(std::string_view("ABCDEF"));
    std::vector<uint8_t> got = in.readBytes(3);
    assert(got.size() == 3);
    assert(got[0] == 'A' && got[1] == 'B' && got[2] == 'C');
    got = in.readBytes(10);
    assert(got.size() == 3);  // only DEF left
    assert(got[0] == 'D' && got[1] == 'E' && got[2] == 'F');
    got = in.readBytes(1);
    assert(got.empty());
}

static void test_readBytesUntilEOF() {
    ConstBufferInputStream in(std::string_view("xy"));
    std::vector<uint8_t> got = in.readBytesUntilEOF();
    assert(got.size() == 2);
    assert(got[0] == 'x' && got[1] == 'y');
    got = in.readBytesUntilEOF();
    assert(got.empty());
}

static void test_readRawLine() {
    ConstBufferInputStream in(std::string_view("line1\nline2\n"));
    std::vector<uint8_t> line1 = in.readRawLine();
    assert(line1.size() == 6);
    assert(line1.back() == '\n');
    std::vector<uint8_t> line2 = in.readRawLine();
    assert(line2.size() == 6);
    std::vector<uint8_t> line3 = in.readRawLine();
    assert(line3.empty());
}

static void test_readRawLineChopped() {
    ConstBufferInputStream in(std::string_view("ab\n"));
    std::vector<uint8_t> line = in.readRawLineChopped();
    assert(line.size() == 2);
    assert(line[0] == 'a' && line[1] == 'b');
}

static void test_skip() {
    ConstBufferInputStream in(std::string_view("12345"));
    assert(in.skip(2) == 2);
    assert(in.read() == '3');
    assert(in.skip(100) == 2);
    assert(in.read() == -1);
}

int main() {
    test_readBytes();
    test_readBytesUntilEOF();
    test_readRawLine();
    test_readRawLineChopped();
    test_skip();
    std::cout << "All InputStream tests passed.\n";
    return 0;
}
