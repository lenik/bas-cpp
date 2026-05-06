#include "StringReader.hpp"

#include <cassert>
#include <iostream>
#include <string_view>

static void test_readChar() {
    StringReader r("Hi");
    assert(r.readChar() == 'H');
    assert(r.readChar() == 'i');
    assert(r.readChar() == -1);
    assert(r.readChar() == -1);
}

static void test_readLine() {
    StringReader r("line1\nline2\n");
    assert(r.readLine() == "line1\n");
    assert(r.readLine() == "line2\n");
    assert(r.readLine().empty());
}

static void test_readLine_no_trailing_newline() {
    StringReader r("only");
    assert(r.readLine() == "only");
    assert(r.readLine().empty());
}

static void test_readLineChopped() {
    // readLineChopped() only strips trailing \r (not \n)
    StringReader r("a\r\nb\n");
    assert(r.readLineChopped() == "a\r\n");  // last char is \n, so no chop
    assert(r.readLineChopped() == "b\n");
    StringReader r2("line\r");
    assert(r2.readLineChopped() == "line");  // trailing \r is removed
}

static void test_readChars() {
    StringReader r("Hello");
    assert(r.readChars(2) == "He");
    assert(r.readChars(10) == "llo");
    assert(r.readChars(1).empty());
}

static void test_readCharsUntilEOF() {
    StringReader r("xyz");
    assert(r.readCharsUntilEOF() == "xyz");
    assert(r.readCharsUntilEOF().empty());
}

static void test_skipChars() {
    StringReader r("abcdef");
    assert(r.skipChars(2) == 2);
    assert(r.readChar() == 'c');
    assert(r.skipChars(100) == 3);
    assert(r.readChar() == -1);
}

static void test_close() {
    StringReader r("ab");
    r.close();
    assert(r.readChar() == -1);
}

int main() {
    test_readChar();
    test_readLine();
    test_readLine_no_trailing_newline();
    test_readLineChopped();
    test_readChars();
    test_readCharsUntilEOF();
    test_skipChars();
    test_close();
    std::cout << "All StringReader tests passed.\n";
    return 0;
}
