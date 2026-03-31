// Unit tests for Uint8ArrayOutputStream

#include "Uint8ArrayOutputStream.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

static void test_write_single() {
    Uint8ArrayOutputStream out;
    assert(out.write(0x41));
    assert(out.write(0x42));
    auto data = out.data();
    assert(data.size() == 2);
    assert(data[0] == 0x41 && data[1] == 0x42);
}

static void test_write_bulk() {
    Uint8ArrayOutputStream out;
    const uint8_t buf[] = {1, 2, 3, 4, 5};
    assert(out.write(buf, 0, 5) == 5);
    assert(out.write(buf, 2, 2) == 2);  // 3,4
    auto data = out.data();
    assert(data.size() == 7);
    assert(data[0] == 1 && data[4] == 5 && data[5] == 3 && data[6] == 4);
}

static void test_close_rejects_write() {
    Uint8ArrayOutputStream out;
    out.write(1);
    out.close();
    assert(!out.write(2));
    assert(out.write(nullptr, 0, 5) == 0);
    auto data = out.data();
    assert(data.size() == 1);
}

static void test_clear() {
    Uint8ArrayOutputStream out;
    out.write(1);
    out.clear();
    assert(out.data().empty());
    assert(out.write(2));  // can write again after clear
    assert(out.data().size() == 1);
}

static void test_flush_noop() {
    Uint8ArrayOutputStream out;
    out.write(1);
    out.flush();
    assert(out.data().size() == 1);
}

int main() {
    test_write_single();
    test_write_bulk();
    test_close_rejects_write();
    test_clear();
    test_flush_noop();
    std::cout << "All Uint8ArrayOutputStream tests passed.\n";
    return 0;
}
