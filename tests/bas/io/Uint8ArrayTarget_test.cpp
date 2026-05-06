// Unit tests for Uint8ArrayTarget

#include "OutputStream.hpp"
#include "Uint8ArrayTarget.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static void test_newOutputStream_writes_bytes() {
    Uint8ArrayTarget target;
    auto out = target.newOutputStream(false);

    assert(out->write(0x41));
    const uint8_t buf[] = {0x42, 0x43};
    assert(out->write(buf, 0, 2) == 2);

    const std::vector<uint8_t>& data = target.array();
    assert(data.size() == 3);
    assert(data[0] == 0x41 && data[1] == 0x42 && data[2] == 0x43);
}

static void test_newOutputStream_append_and_clear() {
    Uint8ArrayTarget target;
    {
        auto out = target.newOutputStream(false);
        out->write(0x10);
    }
    {
        auto out = target.newOutputStream(true);
        out->write(0x20);
    }
    const std::vector<uint8_t>& data = target.array();
    assert(data.size() == 2);
    assert(data[0] == 0x10 && data[1] == 0x20);

    target.clear();
    assert(target.array().empty());
}

static void test_ensureCapacity_and_append() {
    Uint8ArrayTarget target(1);
    target.ensureCapacity(10);
    const uint8_t buf[] = {1, 2, 3, 4};
    target.append(buf, 4);

    const std::vector<uint8_t>& data = target.array();
    assert(data.size() == 4);
    assert(data[0] == 1 && data[3] == 4);
}

static void test_charset_and_getters() {
    Uint8ArrayTarget target(0, "");
    assert(target.getCharset() == "UTF-8");

    target.setCharset("ISO-8859-1");
    assert(target.getCharset() == "ISO-8859-1");

    target.setName("mem");  // setter exists; getName() returns fixed "memory"
    assert(target.getName() == "memory");

    // URI/URL are default-constructed; just ensure calls compile and return something.
    (void)target.getURI();
    (void)target.getURL();
}

int main() {
    test_newOutputStream_writes_bytes();
    test_newOutputStream_append_and_clear();
    test_ensureCapacity_and_append();
    test_charset_and_getters();
    std::cout << "All Uint8ArrayTarget tests passed.\n";
    return 0;
}

