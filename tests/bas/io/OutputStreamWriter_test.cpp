// Unit tests for OutputStreamWriter

#include "OutputStreamWriter.hpp"
#include "Uint8ArrayOutputStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

static void test_write_and_writeln() {
    auto out = std::make_unique<Uint8ArrayOutputStream>();
    Uint8ArrayOutputStream* raw = out.get();
    OutputStreamWriter w(std::move(out), "UTF-8");
    assert(w.charset() == "UTF-8");
    assert(w.write("Hello"));
    assert(w.writeln());
    assert(w.writeln("World"));
    w.flush();
    std::vector<uint8_t> data = raw->data();
    std::string s(data.begin(), data.end());
    assert(s == "Hello\nWorld\n");
    w.close();
}

static void test_writeChar() {
    auto out = std::make_unique<Uint8ArrayOutputStream>();
    Uint8ArrayOutputStream* raw = out.get();
    OutputStreamWriter w(std::move(out));
    assert(w.writeChar('A'));
    assert(w.writeChar('B'));
    w.flush();
    std::vector<uint8_t> data = raw->data();
    assert(data.size() == 2 && data[0] == 'A' && data[1] == 'B');
    w.close();
}

int main() {
    test_write_and_writeln();
    test_writeChar();
    std::cout << "All OutputStreamWriter tests passed.\n";
    return 0;
}
