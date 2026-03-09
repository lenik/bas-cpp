// Unit tests for PrintStream (wraps OutputStream via OutputStreamWriter)

#include "PrintStream.hpp"
#include "Uint8ArrayOutputStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void test_write_and_writeln() {
    auto out = std::make_unique<Uint8ArrayOutputStream>();
    Uint8ArrayOutputStream* raw = out.get();
    PrintStream ps(std::move(out));
    assert(ps.charset() == "UTF-8");
    assert(ps.write("Hello"));
    assert(ps.writeln());
    assert(ps.writeln("World"));
    ps.flush();
    std::vector<uint8_t> data = raw->data();
    std::string s(data.begin(), data.end());
    assert(s == "Hello\nWorld\n");
    ps.close();
}

static void test_writeChar() {
    auto out = std::make_unique<Uint8ArrayOutputStream>();
    Uint8ArrayOutputStream* raw = out.get();
    PrintStream ps(std::move(out));
    assert(ps.writeChar('A'));
    assert(ps.writeChar('B'));
    ps.flush();
    std::vector<uint8_t> data = raw->data();
    assert(data.size() == 2 && data[0] == 'A' && data[1] == 'B');
}

int main() {
    test_write_and_writeln();
    test_writeChar();
    std::cout << "All PrintStream tests passed.\n";
    return 0;
}
