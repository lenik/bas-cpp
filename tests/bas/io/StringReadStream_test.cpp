// Unit tests for StringReadStream

#include "StringReadStream.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_ctor_and_getEncoding() {
    StringReadStream s("hello", "UTF-8");
    assert(s.getEncoding() == "UTF-8");

    StringReadStream s2("data", "ISO-8859-1");
    assert(s2.getEncoding() == "ISO-8859-1");
}

static void test_read() {
    StringReadStream s("AB");
    assert(s.read() == 'A');
    assert(s.read() == 'B');
    assert(s.read() == -1);
    s.close();
}

static void test_readChar() {
    StringReadStream s("xy");
    assert(s.readChar() == 'x');
    assert(s.readChar() == 'y');
    assert(s.readChar() == -1);
    s.close();
}

static void test_readLine() {
    StringReadStream s("first\nsecond\n");
    assert(s.readLine() == "first\n");
    assert(s.readLine() == "second\n");
    assert(s.readLine().empty());
    s.close();
}

static void test_readCharsUntilEOF() {
    StringReadStream s("content");
    assert(s.readCharsUntilEOF() == "content");
    assert(s.readCharsUntilEOF().empty());
    s.close();
}

static void test_empty_string() {
    StringReadStream s("");
    assert(s.read() == -1);
    assert(s.readChar() == -1);
    assert(s.readLine().empty());
    assert(s.readCharsUntilEOF().empty());
    s.close();
}

int main() {
    test_ctor_and_getEncoding();
    test_read();
    test_readChar();
    test_readLine();
    test_readCharsUntilEOF();
    test_empty_string();
    std::cout << "All StringReadStream tests passed.\n";
    return 0;
}
