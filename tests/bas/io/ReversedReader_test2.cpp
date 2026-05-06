#include "ReversedReader.hpp"

#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

/** Minimal buffer that supports backward reads for ReversedReader. */
struct BufferView {
    const uint8_t* data;
    size_t size;
    size_t pos;  // current end; reads consume from pos backward

    BufferView(const uint8_t* d, size_t n) : data(d), size(n), pos(n) {}

    size_t readBackward(uint8_t* buf, size_t end, size_t len) {
        size_t toRead = len;
        if (toRead > pos)
            toRead = pos;
        if (toRead == 0)
            return 0;
        size_t start = pos - toRead;
        memcpy(buf + end - toRead, data + start, toRead);
        pos -= toRead;
        return toRead;
    }
};

namespace {
read_backward_func_t bind_backward(BufferView& view) {
    using namespace std::placeholders;
    return std::bind(&BufferView::readBackward, &view, _1, _2, _3);
}
}  // namespace

static std::vector<uint8_t> g_data;
static size_t g_pos;
static const size_t block_size = 10;

static size_t read_backward_cb(uint8_t* buf, size_t end, size_t len) {
    size_t toRead = len;
    if (toRead > g_pos)
        toRead = g_pos;
    if (toRead == 0)
        return 0;
    size_t start = g_pos - toRead;
    memcpy(buf + end - toRead, g_data.data() + start, toRead);
    g_pos -= toRead;
    return toRead;
}

static void setup(const char* s) {
    g_data.assign(s, s + strlen(s));
    g_pos = g_data.size();
}

// Test2: longer stream, multiple blocks
static void test_longer_stream() {
    setup("0123456789ABCDEF");
    ReversedReader r(read_backward_cb, 4);
    uint8_t buf[16];
    size_t n = r.read(buf, 4, 4);
    assert(n == 4);
    assert(buf[0] == 'C' && buf[1] == 'D' && buf[2] == 'E' && buf[3] == 'F');
    n = r.read(buf, 4, 4);
    assert(n == 4);
    assert(buf[0] == '8' && buf[1] == '9' && buf[2] == 'A' && buf[3] == 'B');
    n = r.read(buf, 4, 4);
    assert(n == 4);
    assert(buf[0] == '4' && buf[1] == '5' && buf[2] == '6' && buf[3] == '7');
    n = r.read(buf, 4, 4);
    assert(n == 4);
    assert(buf[0] == '0' && buf[1] == '1' && buf[2] == '2' && buf[3] == '3');
    n = r.read(buf, 4, 4);
    assert(n == 0);
}

// Test2: read then reject at boundary
static void test_reject_at_boundary() {
    setup("PQRS");
    ReversedReader r(read_backward_cb, 2);
    assert(r.read() == 'S');
    assert(r.read() == 'R');
    assert(r.reject(2));
    assert(r.read() == 'S');
    assert(r.read() == 'R');
    assert(r.read() == 'Q');
    assert(r.read() == 'P');
    assert(r.read() == -1);
}

// Test2: readRawLines empty
static void test_readRawLines_empty() {
    setup("");
    ReversedReader r(read_backward_cb, block_size);
    auto lines = r.readRawLines(-1);
    assert(lines.empty());
}

// Test2: single line no newline
static void test_readRawLines_single_no_newline() {
    setup("only");
    ReversedReader r(read_backward_cb, block_size);
    auto lines = r.readRawLines(-1);
    assert(lines.size() == 1);
    assert(lines[0] == "only");
}

// --- ReversedReader with BufferView (lambda, no global) ---

static void test_buffer_view_single_byte_read() {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    BufferView view(data.data(), data.size());
    ReversedReader r(bind_backward(view), 10);
    assert(r.read() == 'o');
    assert(r.read() == 'l');
    assert(r.read() == 'l');
    assert(r.read() == 'e');
    assert(r.read() == 'H');
    assert(r.read() == -1);
}

static void test_buffer_view_read_buffer() {
    std::vector<uint8_t> data = {'A', 'B', 'C', 'D'};
    BufferView view(data.data(), data.size());
    ReversedReader r(bind_backward(view), 10);
    uint8_t buf[10];
    size_t n = r.read(buf, 2, 2);
    assert(n == 2);
    assert(buf[0] == 'C' && buf[1] == 'D');
    n = r.read(buf, 10, 10);
    assert(n == 2);
    assert(buf[8] == 'A' && buf[9] == 'B');
    n = r.read(buf, 1, 1);
    assert(n == 0);
}

static void test_buffer_view_read_raw_line() {
    std::string s = "line1\nline2\n";
    std::vector<uint8_t> data(s.begin(), s.end());
    BufferView view(data.data(), data.size());
    ReversedReader r(bind_backward(view), 10);
    assert(r.readRawLine() == "line2\n");
    assert(r.readRawLine() == "line1\n");
    assert(r.readRawLine().empty());
}

static void test_buffer_view_empty() {
    std::vector<uint8_t> data;
    BufferView view(data.data(), data.size());
    ReversedReader r(bind_backward(view), 10);
    assert(r.read() == -1);
    uint8_t buf[4];
    assert(r.read(buf, 4, 4) == 0);
}

int main() {
    test_longer_stream();
    test_reject_at_boundary();
    test_readRawLines_empty();
    test_readRawLines_single_no_newline();
    test_buffer_view_single_byte_read();
    test_buffer_view_read_buffer();
    test_buffer_view_read_raw_line();
    test_buffer_view_empty();
    std::cout << "All ReversedReader test2 passed.\n";
    return 0;
}
