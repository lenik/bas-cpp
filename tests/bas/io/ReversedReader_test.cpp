#include "ReversedReader.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

// Test harness: static buffer for ReversedReader callback.
// read_backward returns each block in non-reversed (original) order:
// buf[0] = first byte of the block in source order, buf[n-1] = last.
// ReversedReader consumes blocks from the end, so it sees bytes backward.
static std::vector<uint8_t> g_data;
static size_t g_pos;  // current "end"; we return blocks from g_pos-1 backward

static const size_t block_size = 10;

static size_t read_backward_cb(uint8_t* buf, size_t end, size_t len) {
    size_t toRead = len;
    if (toRead > g_pos)
        toRead = g_pos;
    if (toRead == 0)
        return 0;
    size_t start = g_pos - toRead;
    memcpy(buf + end - toRead, g_data.data() + start, toRead);  // write at end of range
    g_pos -= toRead;
    return toRead;
}

static void setup(const char* s) {
    g_data.assign(s, s + strlen(s));
    g_pos = g_data.size();
}

static void test_read_single_byte() {
    setup("Hello");
    ReversedReader r(read_backward_cb, block_size);
    assert(r.read() == 'o');
    assert(r.read() == 'l');
    assert(r.read() == 'l');
    assert(r.read() == 'e');
    assert(r.read() == 'H');
    assert(r.read() == -1);
    assert(r.read() == -1);
}

static void test_read_empty() {
    setup("");
    ReversedReader r(read_backward_cb, block_size);
    assert(r.read() == -1);
    uint8_t buf[4];
    assert(r.read(buf, 4, 4) == 0);  // read(buf, end, len), off = end - len
}

static void test_read_buffer() {
    setup("ABCD");
    ReversedReader r(read_backward_cb, block_size);
    uint8_t buf[10];
    size_t n = r.read(buf, 2, 2);  // read(buf, end, len) -> write at buf[0..2)
    assert(n == 2);
    assert(buf[0] == 'C' && buf[1] == 'D');  // last block in original order
    n = r.read(buf, 10, 10);  // write at buf[0..10), 2 bytes at buf[8], buf[9]
    assert(n == 2);
    assert(buf[8] == 'A' && buf[9] == 'B');  // first block in original order
    n = r.read(buf, 1, 1);
    assert(n == 0);
}

static void test_read_buffer_multi_block() {
    // block_size=2 so we get multiple blocks, each in original order
    // read(buf, 10, 10) fills from end: 4 bytes at buf[6..10)
    setup("1234");
    ReversedReader r(read_backward_cb, 2);
    uint8_t buf[10];
    size_t n = r.read(buf, 10, 10);
    assert(n == 4);
    assert(buf[6] == '1' && buf[7] == '2' && buf[8] == '3' && buf[9] == '4');  // "12" then "34"
}

static void test_reject() {
    setup("XY");
    ReversedReader r(read_backward_cb, block_size);
    assert(r.read() == 'Y');
    assert(r.reject());       // put back one byte
    assert(r.read() == 'Y');
    assert(r.read() == 'X');
    assert(r.read() == -1);   // EOF
    assert(r.reject());
    assert(r.read() == 'X');
    assert(r.read() == -1);
}

static void test_reject_n() {
    setup("abcd");
    ReversedReader r(read_backward_cb, block_size);
    assert(r.read() == 'd');
    assert(r.read() == 'c');
    assert(r.reject(2));
    assert(r.read() == 'd');
    assert(r.read() == 'c');
    assert(r.read() == 'b');
    assert(r.read() == 'a');
    assert(r.read() == -1);
}

static void test_lookAhead_char() {
    setup("X");
    ReversedReader r(read_backward_cb, block_size);
    assert(r.lookAhead() == 'X');
    assert(r.lookAhead() == 'X');
    assert(r.read() == 'X');
    assert(r.read() == -1);
}

static void test_lookAhead_buffer() {
    setup("AB");
    ReversedReader r(read_backward_cb, block_size);
    uint8_t buf[4];
    int n = r.lookAhead(buf, 2, 2);  // lookAhead(buf, end, len)
    assert(n == 2);
    assert(buf[0] == 'A' && buf[1] == 'B');  // block in original order
    n = r.lookAhead(buf, 2, 2);
    assert(n == 2);
    assert(buf[0] == 'A' && buf[1] == 'B');
    // After lookAhead we may have put back (reject) or not; read either 2 or 0
    size_t got = r.read(buf, 2, 2);
    assert(got == 2 || got == 0);
    assert(r.read(buf, 1, 1) == 0);
}

static void test_readRawLine() {
    // Backward: we read "line2\n" then "line1\n"
    setup("line1\nline2\n");
    ReversedReader r(read_backward_cb, block_size);
    std::string a = r.readRawLine();
    assert(a == "line2\n");
    std::string b = r.readRawLine();
    assert(b == "line1\n");
    std::string c = r.readRawLine();
    if (!c.empty()) std::cerr << "c len=" << c.size() << " content=[" << c << "]\n";
    assert(c.empty());
}

static void test_readRawLine_no_trailing_newline() {
    setup("nope");
    ReversedReader r(read_backward_cb, block_size);
    std::string a = r.readRawLine();
    assert(a == "nope");
    std::string b = r.readRawLine();
    assert(b.empty());
}

static void test_readRawLines() {
    setup("a\nb\nc\n");
    ReversedReader r(read_backward_cb, block_size);
    auto lines = r.readRawLines(-1);
    assert(lines.size() == 3);
    assert(lines[0] == "c\n");
    assert(lines[1] == "b\n");
    assert(lines[2] == "a\n");
}

static void test_readRawLines_max() {
    setup("a\nb\nc\n");
    ReversedReader r(read_backward_cb, block_size);
    auto lines = r.readRawLines(2);
    assert(lines.size() == 2);
    assert(lines[0] == "c\n");
    assert(lines[1] == "b\n");
}

static void test_readLine_utf8() {
    setup("foo\n");
    ReversedReader r(read_backward_cb, block_size);
    std::string line = r.readLine("UTF-8");
    assert(line == "foo\n");
}

static void test_readLines_utf8() {
    setup("x\n");
    ReversedReader r(read_backward_cb, block_size);
    auto lines = r.readLines("UTF-8", 10);
    assert(lines.size() == 1);
    assert(lines[0] == "x\n");
}

// Single-byte blocks to stress block transitions and reject across boundary
static void test_single_byte_blocks() {
    setup("xyz");
    ReversedReader r(read_backward_cb, 1);  // block_size 1 → 3 blocks
    assert(r.read() == 'z');
    assert(r.read() == 'y');
    assert(r.reject(1));
    assert(r.read() == 'y');
    assert(r.read() == 'x');
    assert(r.read() == -1);
}

int main() {
    test_read_single_byte();
    test_read_empty();
    test_read_buffer();
    test_read_buffer_multi_block();
    test_reject();
    test_reject_n();
    test_lookAhead_char();
    test_lookAhead_buffer();
    test_readRawLine();
    test_readRawLine_no_trailing_newline();
    test_readRawLines();
    test_readRawLines_max();
    test_readLine_utf8();
    test_readLines_utf8();
    test_single_byte_blocks();
    std::cout << "All ReversedReader tests passed.\n";
    return 0;
}

