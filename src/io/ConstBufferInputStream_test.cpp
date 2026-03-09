#include "ConstBufferInputStream.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string_view>
#include <vector>

static void test_read_single() {
    const char* data = "Hi";
    ConstBufferInputStream in(reinterpret_cast<const uint8_t*>(data), 2);
    assert(in.read() == 'H');
    assert(in.read() == 'i');
    assert(in.read() == -1);
    assert(in.read() == -1);
}

static void test_read_string_view() {
    ConstBufferInputStream in(std::string_view("AB"));
    assert(in.read() == 'A');
    assert(in.read() == 'B');
    assert(in.read() == -1);
}

static void test_read_bulk() {
    const uint8_t data[] = {1, 2, 3, 4, 5};
    ConstBufferInputStream in(data, 5);
    uint8_t buf[10] = {0};
    size_t n = in.read(buf, 0, 3);
    assert(n == 3);
    assert(buf[0] == 1 && buf[1] == 2 && buf[2] == 3);
    n = in.read(buf, 3, 5);
    assert(n == 2);
    assert(buf[3] == 4 && buf[4] == 5);
    n = in.read(buf, 0, 1);
    assert(n == 0);
}

static void test_skip() {
    ConstBufferInputStream in(std::string_view("abcdef"));
    assert(in.skip(2) == 2);
    assert(in.read() == 'c');
    assert(in.skip(10) == 3);
    assert(in.read() == -1);
}

static void test_position_seek() {
    ConstBufferInputStream in(std::string_view("xyz"));
    assert(in.position() == 0);
    assert(in.read() == 'x');
    assert(in.position() == 1);
    assert(in.canSeek(std::ios::beg));
    assert(in.seek(0, std::ios::beg));
    assert(in.position() == 0);
    assert(in.read() == 'x');
    in.seek(2, std::ios::beg);
    assert(in.read() == 'z');
    assert(in.read() == -1);
}

static void test_close() {
    ConstBufferInputStream in(std::string_view("a"));
    in.close();
    assert(in.read() == -1);
    assert(in.position() == -1);
    assert(!in.canSeek(std::ios::beg));
}

static void test_readBytes_readRawLine() {
    ConstBufferInputStream in(std::string_view("line1\nline2"));
    std::vector<uint8_t> line = in.readRawLine();
    assert(line.size() == 6);
    assert(memcmp(line.data(), "line1\n", 6) == 0);
    line = in.readRawLine();
    assert(line.size() == 5);
    assert(memcmp(line.data(), "line2", 5) == 0);
    line = in.readRawLine();
    assert(line.empty());
}

int main() {
    test_read_single();
    test_read_string_view();
    test_read_bulk();
    test_skip();
    test_position_seek();
    test_close();
    test_readBytes_readRawLine();
    std::cout << "All ConstBufferInputStream tests passed.\n";
    return 0;
}
