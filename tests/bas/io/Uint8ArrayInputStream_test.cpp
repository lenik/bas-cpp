// Unit tests for Uint8ArrayInputStream

#include "Uint8ArrayInputStream.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

static void test_slice_constructor() {
    std::vector<uint8_t> data = {10, 20, 30, 40, 50};
    Uint8ArrayInputStream in(data, 1, 3);  // 20,30,40
    assert(in.read() == 20);
    assert(in.read() == 30);
    assert(in.read() == 40);
    assert(in.read() == -1);
}

static void test_slice_len_npos_clamped() {
    std::vector<uint8_t> data = {1, 2, 3};
    Uint8ArrayInputStream in(data, 1, Uint8ArrayInputStream::npos);  // 2,3
    uint8_t buf[4] = {0};
    size_t n = in.read(buf, 0, 4);
    assert(n == 2);
    assert(buf[0] == 2 && buf[1] == 3);
    assert(in.read() == -1);
}

static void test_move_constructor_and_data() {
    std::vector<uint8_t> data = {7, 8, 9};
    Uint8ArrayInputStream in(std::move(data));
    auto copy = in.data();
    assert(copy.size() == 3);
    assert(copy[0] == 7 && copy[1] == 8 && copy[2] == 9);
}

static void test_ptr_constructor_and_read_bulk() {
    const uint8_t raw[] = {42, 43, 44, 45};
    Uint8ArrayInputStream in(raw, 4);
    uint8_t buf[4] = {0};
    size_t n = in.read(buf, 1, 3);
    assert(n == 3);
    assert(buf[1] == 42 && buf[2] == 43 && buf[3] == 44);
    assert(in.read() == 45);
    assert(in.read() == -1);
}

static void test_skip_and_position() {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    Uint8ArrayInputStream in(data);
    assert(in.position() == 0);
    assert(in.skip(2) == 2);
    assert(in.position() == 2);
    assert(in.read() == 3);
    assert(in.skip(10) == 2);  // 4,5
    assert(in.read() == -1);
}

static void test_seek() {
    std::vector<uint8_t> data = {10, 20, 30, 40};
    Uint8ArrayInputStream in(data);
    assert(in.seek(2, std::ios::beg));
    assert(in.read() == 30);
    // After reading 30, position is 3; seek back one and read 30 again
    assert(in.seek(-1, std::ios::cur));
    assert(in.read() == 30);
    assert(in.seek(-4, std::ios::end));  // back to 0
    assert(in.read() == 10);
    assert(!in.seek(10, std::ios::beg)); // out of range
}

static void test_close() {
    std::vector<uint8_t> data = {1};
    Uint8ArrayInputStream in(data);
    in.close();
    assert(in.read() == -1);
    assert(in.position() == -1);
    assert(!in.canSeek(std::ios::beg));
}

int main() {
    test_slice_constructor();
    test_slice_len_npos_clamped();
    test_move_constructor_and_data();
    test_ptr_constructor_and_read_bulk();
    test_skip_and_position();
    test_seek();
    test_close();
    std::cout << "All Uint8ArrayInputStream tests passed.\n";
    return 0;
}

