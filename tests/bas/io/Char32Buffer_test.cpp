// Unit tests for Char32Buffer

#include "Char32Buffer.hpp"

#include <cassert>
#include <iostream>

static void test_allocate_and_capacity() {
    Char32Buffer b = Char32Buffer::allocate(16);
    assert(b.capacity() == 16);
    assert(b.position() == 0);
    assert(b.limit() == 16);
    assert(b.remaining() == 16);
    assert(b.hasRemaining());
}

static void test_put_get() {
    Char32Buffer b = Char32Buffer::allocate(4);
    b.put(U'A').put(U'B').put(U'C').put(U'D');
    assert(b.position() == 4);
    assert(!b.hasRemaining());
    b.flip();
    assert(b.remaining() == 4);
    assert(b.get() == U'A');
    assert(b.get() == U'B');
    assert(b.get() == U'C');
    assert(b.get() == U'D');
    assert(b.remaining() == 0);
    assert(b.get() == 0);
}

static void test_put_get_bulk() {
    Char32Buffer b = Char32Buffer::allocate(10);
    char32_t in[] = {U'a', U'b', U'c', U'd', U'e'};
    b.put(in, 5);
    assert(b.position() == 5);
    b.flip();
    char32_t out[10] = {0};
    b.get(out, 3);
    assert(out[0] == U'a' && out[1] == U'b' && out[2] == U'c');
    assert(b.position() == 3);
    b.get(out + 3, 5);
    assert(out[3] == U'd' && out[4] == U'e');
    assert(b.remaining() == 0);
}

static void test_clear_flip_rewind() {
    Char32Buffer b = Char32Buffer::allocate(4);
    b.put(U'x').put(U'y');
    assert(b.position() == 2);
    b.flip();
    assert(b.limit() == 2);
    assert(b.position() == 0);
    assert(b.get() == U'x');
    b.rewind();
    assert(b.position() == 0);
    assert(b.get() == U'x');
    b.clear();
    assert(b.position() == 0);
    assert(b.limit() == 4);
}

static void test_compact() {
    Char32Buffer b = Char32Buffer::allocate(6);
    b.put(U'1').put(U'2').put(U'3').put(U'4').put(U'5').put(U'6');
    b.flip();
    assert(b.get() == U'1');
    assert(b.get() == U'2');
    b.compact();
    assert(b.position() == 4);
    assert(b.limit() == 6);
    assert(b.remaining() == 2);
    b.flip();
    assert(b.get() == U'3');
    assert(b.get() == U'4');
}

static void test_position_limit_clamp() {
    Char32Buffer b = Char32Buffer::allocate(4);
    b.position(10);
    assert(b.position() == 4);
    b.limit(2);
    assert(b.limit() == 2);
    assert(b.position() == 2);
}

int main() {
    test_allocate_and_capacity();
    test_put_get();
    test_put_get_bulk();
    test_clear_flip_rewind();
    test_compact();
    test_position_limit_clamp();
    std::cout << "All Char32Buffer tests passed.\n";
    return 0;
}
