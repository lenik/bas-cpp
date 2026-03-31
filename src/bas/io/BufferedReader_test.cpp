// Unit tests for BufferedReader

#include "BufferedReader.hpp"
#include "StringReader.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_readChar_and_readChars() {
    auto in = std::make_unique<StringReader>("ABC");
    BufferedReader r(std::move(in));

    assert(r.readChar() == 'A');
    assert(r.readChar() == 'B');
    assert(r.readChars(2) == "C");  // only 1 left, then EOF
    assert(r.readChar() == -1);
    r.close();
}

static void test_readCharsUntilEOF() {
    auto in = std::make_unique<StringReader>("xyz");
    BufferedReader r(std::move(in));
    assert(r.readCharsUntilEOF() == "xyz");
    assert(r.readCharsUntilEOF().empty());
    r.close();
}

static void test_readLine() {
    auto in = std::make_unique<StringReader>("L1\nL2\n");
    BufferedReader r(std::move(in));
    assert(r.readLine() == "L1\n");
    assert(r.readLine() == "L2\n");
    assert(r.readLine().empty());
    r.close();
}

static void test_close() {
    auto in = std::make_unique<StringReader>("x");
    BufferedReader r(std::move(in));
    r.close();
    assert(r.readChar() == -1);
}

int main() {
    test_readChar_and_readChars();
    test_readCharsUntilEOF();
    test_readLine();
    test_close();
    std::cout << "All BufferedReader tests passed.\n";
    return 0;
}
