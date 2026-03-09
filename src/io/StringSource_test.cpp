// Unit tests for StringSource (test newReader() only; newRandomReader uses U32stringReader(string) which has lifetime issues)

#include "StringSource.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_newReader_reads_content() {
    StringSource src("Hello");
    auto r = src.newReader();
    assert(r);
    assert(r->readCharsUntilEOF() == "Hello");
    r->close();
}

static void test_getCharset_and_setters() {
    StringSource src("x", "UTF-8");
    assert(src.getCharset() == "UTF-8");
    src.setCharset("ISO-8859-1");
    assert(src.getCharset() == "ISO-8859-1");
    src.setName("buf");
    assert(src.getName() == "buf");
    (void)src.getURI();
    (void)src.getURL();
}

static void test_empty_string() {
    StringSource src("");
    auto r = src.newReader();
    assert(r);
    assert(r->readCharsUntilEOF().empty());
    assert(r->readChar() == -1);
}

int main() {
    test_newReader_reads_content();
    test_getCharset_and_setters();
    test_empty_string();
    std::cout << "All StringSource tests passed.\n";
    return 0;
}
