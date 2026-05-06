#include "CharBuffer.hpp"

#include <cassert>
#include <iostream>

static void test_allocate_and_capacity() {
    CharBuffer b = CharBuffer::allocate(16);
    assert(b.capacity() == 16);
    assert(b.position() == 0);
    assert(b.limit() == 16);
    assert(b.remaining() == 16);
    assert(b.hasRemaining());
}

static void test_put_get() {
    CharBuffer b = CharBuffer::allocate(4);
    b.put('A').put('B').put('C').put('D');
    assert(b.position() == 4);
    assert(!b.hasRemaining());
    b.flip();
    assert(b.remaining() == 4);
    assert(b.get() == 'A');
    assert(b.get() == 'B');
    assert(b.get() == 'C');
    assert(b.get() == 'D');
    assert(b.remaining() == 0);
    assert(b.get() == 0);
}

static void test_put_get_bulk() {
    CharBuffer b = CharBuffer::allocate(10);
    char16_t in[] = {'a', 'b', 'c', 'd', 'e'};
    b.put(in, 5);
    assert(b.position() == 5);
    b.flip();
    char16_t out[10] = {0};
    b.get(out, 3);
    assert(out[0] == 'a' && out[1] == 'b' && out[2] == 'c');
    assert(b.position() == 3);
    b.get(out + 3, 5);
    assert(out[3] == 'd' && out[4] == 'e');
    assert(b.remaining() == 0);
}

static void test_clear_flip_rewind() {
    CharBuffer b = CharBuffer::allocate(4);
    b.put('x').put('y');
    assert(b.position() == 2);
    b.flip();
    assert(b.limit() == 2);
    assert(b.position() == 0);
    assert(b.get() == 'x');
    b.rewind();
    assert(b.position() == 0);
    assert(b.get() == 'x');
    b.clear();
    assert(b.position() == 0);
    assert(b.limit() == 4);
}

static void test_compact() {
    CharBuffer b = CharBuffer::allocate(6);
    b.put('1').put('2').put('3').put('4').put('5').put('6');
    b.flip();
    assert(b.get() == '1');
    assert(b.get() == '2');
    assert(b.position() == 2);
    b.compact();
    assert(b.position() == 4);
    assert(b.limit() == 6);
    assert(b.data()[0] == '3' && b.data()[1] == '4' && b.data()[2] == '5' && b.data()[3] == '6');
}

static void test_position_limit() {
    CharBuffer b = CharBuffer::allocate(8);
    b.position(4);
    assert(b.position() == 4);
    b.limit(6);
    assert(b.limit() == 6);
    assert(b.remaining() == 2);
    b.position(10);
    assert(b.position() == 6);
}

int main() {
    test_allocate_and_capacity();
    test_put_get();
    test_put_get_bulk();
    test_clear_flip_rewind();
    test_compact();
    test_position_limit();
    std::cout << "All CharBuffer tests passed.\n";
    return 0;
}
