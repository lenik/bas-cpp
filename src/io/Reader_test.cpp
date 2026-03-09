// Unit tests for Reader interface (tested via StringReader)

#include "Reader.hpp"
#include "StringReader.hpp"

#include <cassert>
#include <iostream>
#include <memory>

static void test_reader_interface_readLine() {
    std::unique_ptr<Reader> r = std::make_unique<StringReader>("a\nb\n");
    assert(r->readLine() == "a\n");
    assert(r->readLine() == "b\n");
    assert(r->readLine().empty());
}

static void test_reader_interface_readChar() {
    std::unique_ptr<Reader> r = std::make_unique<StringReader>("XY");
    assert(r->readChar() == 'X');
    assert(r->readChar() == 'Y');
    assert(r->readChar() == -1);
}

static void test_reader_interface_close() {
    std::unique_ptr<Reader> r = std::make_unique<StringReader>("x");
    r->close();
    assert(r->readChar() == -1);
}

int main() {
    test_reader_interface_readLine();
    test_reader_interface_readChar();
    test_reader_interface_close();
    std::cout << "All Reader tests passed.\n";
    return 0;
}
