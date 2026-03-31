// Unit tests for WstringWriter

#include "WstringWriter.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_writeChar_and_str() {
    WstringWriter w;
    assert(w.writeChar(U'A'));
    assert(w.writeChar(U'B'));
    std::wstring s = w.str();
    assert(s.size() == 2u);
    assert(s[0] == L'A' && s[1] == L'B');
}

static void test_write_utf8() {
    WstringWriter w;
    assert(w.write("Hi"));
    std::wstring s = w.str();
    assert(s.size() == 2u);
    assert(s[0] == L'H' && s[1] == L'i');
    assert(w.write("\xc3\xa9"));  // UTF-8 é
    s = w.str();
    assert(s.size() == 3u);
    assert(s[2] == L'\u00E9');
}

static void test_writeln() {
    WstringWriter w;
    assert(w.writeln("ab"));
    std::wstring s = w.str();
    assert(s.size() == 3u);
    assert(s[0] == L'a' && s[1] == L'b' && s[2] == L'\n');
    assert(w.writeln());
    assert(w.str().size() == 4u);
}

static void test_flush_close_noop() {
    WstringWriter w;
    w.write("x");
    w.flush();
    w.close();
    assert(w.str().size() == 1u);
}

static void test_empty_write() {
    WstringWriter w;
    assert(w.write(""));
    assert(w.str().empty());
}

int main() {
    test_writeChar_and_str();
    test_write_utf8();
    test_writeln();
    test_flush_close_noop();
    test_empty_write();
    std::cout << "All WstringWriter tests passed.\n";
    return 0;
}
