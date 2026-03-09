// Unit tests for U32stringWriter

#include "U32stringWriter.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_writeChar_and_write() {
    U32stringWriter w;
    assert(w.writeChar(U'A'));
    assert(w.writeChar(U'B'));
    assert(w.write("CD"));
    std::u32string s = w.str();
    assert(s.size() == 4);
    assert(s[0] == U'A' && s[1] == U'B' && s[2] == U'C' && s[3] == U'D');
}

static void test_writeln() {
    U32stringWriter w;
    assert(w.writeln("Hi"));
    std::u32string s = w.str();
    assert(s.size() == 3);
    assert(s[0] == U'H' && s[1] == U'i' && s[2] == U'\n');
    assert(w.writeln());
    assert(w.str().size() == 4);
}

static void test_invalid_codePoint_returns_false() {
    U32stringWriter w;
    assert(!w.writeChar(static_cast<char32_t>(-1)));
    assert(!w.writeChar(0x110000));  // beyond Unicode range
    assert(w.str().empty());
}

static void test_flush_close_noop() {
    U32stringWriter w;
    w.write("x");
    w.flush();
    w.close();
    assert(w.str().size() == 1);
}

int main() {
    test_writeChar_and_write();
    test_writeln();
    test_invalid_codePoint_returns_false();
    test_flush_close_noop();
    std::cout << "All U32stringWriter tests passed.\n";
    return 0;
}
