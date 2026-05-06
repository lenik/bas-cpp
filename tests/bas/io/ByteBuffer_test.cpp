#include "ByteBuffer.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

static void test_allocate_and_capacity() {
    ByteBuffer b = ByteBuffer::allocate(16);
    assert(b.capacity() == 16);
    assert(b.position() == 0);
    assert(b.limit() == 16);
    assert(b.remaining() == 16);
    assert(b.hasRemaining());
}

static void test_put_get() {
    ByteBuffer b = ByteBuffer::allocate(4);
    b.put(0x01).put(0x02).put(0x03).put(0x04);
    assert(b.position() == 4);
    assert(!b.hasRemaining());
    b.flip();
    assert(b.remaining() == 4);
    assert(b.get() == 0x01);
    assert(b.get() == 0x02);
    assert(b.get() == 0x03);
    assert(b.get() == 0x04);
    assert(b.remaining() == 0);
    assert(b.get() == 0);  // past end returns 0
}

static void test_put_get_bulk() {
    ByteBuffer b = ByteBuffer::allocate(10);
    uint8_t in[] = {1, 2, 3, 4, 5};
    b.put(in, 5);
    assert(b.position() == 5);
    b.flip();
    uint8_t out[10] = {0};
    b.get(out, 3);
    assert(out[0] == 1 && out[1] == 2 && out[2] == 3);
    assert(b.position() == 3);
    b.get(out + 3, 5);
    assert(out[3] == 4 && out[4] == 5);
    assert(b.remaining() == 0);
}

static void test_clear_flip_rewind() {
    ByteBuffer b = ByteBuffer::allocate(4);
    b.put(0xaa).put(0xbb);
    assert(b.position() == 2);
    b.flip();
    assert(b.limit() == 2);
    assert(b.position() == 0);
    assert(b.get() == 0xaa);
    b.rewind();
    assert(b.position() == 0);
    assert(b.get() == 0xaa);
    b.clear();
    assert(b.position() == 0);
    assert(b.limit() == 4);
}

static void test_compact() {
    ByteBuffer b = ByteBuffer::allocate(6);
    b.put(1).put(2).put(3).put(4).put(5).put(6);
    b.flip();
    assert(b.get() == 1);
    assert(b.get() == 2);
    assert(b.position() == 2);
    b.compact();
    assert(b.position() == 4);
    assert(b.limit() == 6);
    assert(b.data()[0] == 3 && b.data()[1] == 4 && b.data()[2] == 5 && b.data()[3] == 6);
}

static void test_position_limit() {
    ByteBuffer b = ByteBuffer::allocate(8);
    b.position(4);
    assert(b.position() == 4);
    b.limit(6);
    assert(b.limit() == 6);
    assert(b.remaining() == 2);
    b.position(10);  // clamped to limit
    assert(b.position() == 6);
}

static void test_put_past_limit() {
    ByteBuffer b = ByteBuffer::allocate(2);
    b.put(0x11).put(0x22).put(0x33);
    assert(b.position() == 2);
    assert(b.data()[0] == 0x11 && b.data()[1] == 0x22);
}

static void test_ptr() {
    ByteBuffer b = ByteBuffer::allocate(4);
    b.put(0x41).put(0x42);
    assert(b.ptr() == b.data() + 2);
    b.flip();
    assert(b.ptr() == b.data());
}

int main() {
    test_allocate_and_capacity();
    test_put_get();
    test_put_get_bulk();
    test_clear_flip_rewind();
    test_compact();
    test_position_limit();
    test_put_past_limit();
    test_ptr();
    std::cout << "All ByteBuffer tests passed.\n";
    return 0;
}
