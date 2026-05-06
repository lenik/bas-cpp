// Unit tests for LAInputStreamFromStream (LAInputStream wrapping a seekable stream)

#include "ConstBufferInputStream.hpp"
#include "LAInputStreamFromStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_read_and_lookAhead() {
    std::string data = "ABCD";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 4);

    assert(in.lookAhead() == 'A');
    assert(in.read() == 'A');
    assert(in.lookAhead() == 'B');
    assert(in.read() == 'B');
    assert(in.read() == 'C');
    assert(in.read() == 'D');
    assert(in.read() == -1);
    assert(in.lookAhead() == -1);
    in.close();
}

static void test_reject_and_position() {
    std::string data = "XY";
    auto raw = std::make_unique<ConstBufferInputStream>(data);
    LAInputStreamFromStream in(std::move(raw), 4);

    assert(in.position() == 0);
    assert(in.read() == 'X');
    assert(in.position() == 1);
    assert(in.reject(1));
    assert(in.position() == 0);
    assert(in.read() == 'X');
    assert(in.read() == 'Y');
    assert(in.read() == -1);
    in.close();
}

int main() {
    test_read_and_lookAhead();
    test_reject_and_position();
    std::cout << "All LAInputStreamFromStream tests passed.\n";
    return 0;
}
