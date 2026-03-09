// Unit tests for U32stringSource

#include "U32stringSource.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_u32_construct_newReader() {
    std::u32string u32 = {U'A', U'B', U'C'};
    U32stringSource src(u32);
    auto r = src.newReader();
    assert(r);
    assert(r->readCharsUntilEOF() == "ABC");
    r->close();
}

static void test_utf8_construct_newReader() {
    U32stringSource src("Hi");
    auto r = src.newReader();
    assert(r);
    assert(r->readCharsUntilEOF() == "Hi");
    r->close();
}

static void test_newRandomReader() {
    std::u32string u32 = {U'x', U'y', U'z'};
    U32stringSource src(u32);
    auto r = src.newRandomReader();
    assert(r);
    assert(r->readChar() == 'x');
    assert(r->readChars(2) == "yz");
    assert(r->readChar() == -1);
}

static void test_data_and_getters() {
    std::u32string u32 = {U'1'};
    U32stringSource src(u32);
    assert(src.data().size() == 1 && src.data()[0] == U'1');
    assert(src.getCharset() == "UTF-8");
    src.setCharset("UTF-16");
    assert(src.getCharset() == "UTF-16");
    src.setName("u32");
    assert(src.getName() == "u32");
}

int main() {
    test_u32_construct_newReader();
    test_utf8_construct_newReader();
    test_newRandomReader();
    test_data_and_getters();
    std::cout << "All U32stringSource tests passed.\n";
    return 0;
}
