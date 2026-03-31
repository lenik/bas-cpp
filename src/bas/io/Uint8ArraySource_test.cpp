// Unit tests for Uint8ArraySource

#include "Uint8ArraySource.hpp"

#include <cassert>
#include <iostream>
#include <string_view>
#include <vector>

static void test_construct_full_array() {
    std::vector<uint8_t> data = {1, 2, 3, 4};
    Uint8ArraySource src(data, 0, static_cast<size_t>(-1), "UTF-8");

    auto in = src.newRandomInputStream();
    std::vector<uint8_t> buf = in->readBytesUntilEOF();
    assert(buf.size() == 4);
    assert(buf[0] == 1 && buf[3] == 4);
}

static void test_construct_with_offset_and_len() {
    std::vector<uint8_t> data = {10, 20, 30, 40, 50};
    Uint8ArraySource src(data, 1, 3, "UTF-8"); // 20,30,40

    auto in = src.newInputStream();
    std::vector<uint8_t> buf = in->readBytesUntilEOF();
    assert(buf.size() == 3);
    assert(buf[0] == 20 && buf[2] == 40);
}

static void test_len_clamped_to_array_size() {
    std::vector<uint8_t> data = {5, 6};
    Uint8ArraySource src(data, 1, 10, "UTF-8"); // off+len > size -> clamp

    auto in = src.newRandomInputStream();
    std::vector<uint8_t> buf = in->readBytesUntilEOF();
    assert(buf.size() == 1);
    assert(buf[0] == 6);
}

static void test_offset_past_end_yields_empty() {
    std::vector<uint8_t> data = {1, 2, 3};
    Uint8ArraySource src(data, 10, static_cast<size_t>(-1), "UTF-8");

    auto in = src.newInputStream();
    std::vector<uint8_t> buf = in->readBytesUntilEOF();
    assert(buf.empty());
}

static void test_default_charset_and_getters() {
    std::vector<uint8_t> data = { 'A', 'B' };
    Uint8ArraySource src(data, 0, static_cast<size_t>(-1), "");

    assert(src.getCharset() == "UTF-8");
    // URI/URL/name are default-constructed; just ensure calls compile and return something.
    (void)src.getURI();
    (void)src.getURL();
    (void)src.getName();
}

int main() {
    test_construct_full_array();
    test_construct_with_offset_and_len();
    test_len_clamped_to_array_size();
    test_offset_past_end_yields_empty();
    test_default_charset_and_getters();
    std::cout << "All Uint8ArraySource tests passed.\n";
    return 0;
}

