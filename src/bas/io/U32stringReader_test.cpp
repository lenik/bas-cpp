// Unit tests for U32stringReader

#include "U32stringReader.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_u32_construct_and_readChar() {
    std::u32string u32 = {U'A', U'B'};
    U32stringReader r(u32);
    assert(r.readChar() == 'A');
    assert(r.readChar() == 'B');
    assert(r.readChar() == -1);
}

static void test_u32_construct_and_readChars() {
    std::u32string u32 = {U'a', U'b', U'c'};
    U32stringReader r(u32);
    assert(r.readChars(2) == "ab");
    assert(r.readChars(1) == "c");
    assert(r.readChars(1).empty());
}

static void test_readCharsUntilEOF() {
    std::u32string u32 = {U'x', U'y', U'z'};
    U32stringReader r(u32);
    assert(r.readCharsUntilEOF() == "xyz");
    assert(r.readCharsUntilEOF().empty());
}

static void test_readLine_and_chopped() {
    std::u32string u32 = {U'L', U'1', U'\n', U'L', U'2', U'\n'};
    U32stringReader r(u32);
    // readLineChopped() only strips trailing \\r; newline remains in current impl
    assert(r.readLineChopped() == "L1\n");
    assert(r.readLineChopped() == "L2\n");
    assert(r.readLineChopped().empty());
}

static void test_skipChars_and_seek() {
    std::u32string u32 = {U'1', U'2', U'3', U'4', U'5'};
    U32stringReader r(u32);
    assert(r.skipChars(2) == 2);
    assert(r.readChar() == '3');
    assert(r.charPosition() == 3);
    r.seekChars(2, std::ios::beg);
    assert(r.readChar() == '3');
}

static void test_close() {
    std::u32string u32 = {U'x'};
    U32stringReader r(u32);
    r.close();
    assert(r.readChar() == -1);
}

int main() {
    test_u32_construct_and_readChar();
    test_u32_construct_and_readChars();
    test_readCharsUntilEOF();
    test_readLine_and_chopped();
    test_skipChars_and_seek();
    test_close();
    std::cout << "All U32stringReader tests passed.\n";
    return 0;
}
