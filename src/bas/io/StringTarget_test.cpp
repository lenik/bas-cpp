// Unit tests for StringTarget

#include "OutputStream.hpp"
#include "StringTarget.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_newOutputStream_and_str() {
    StringTarget target;
    auto out = target.newOutputStream(false);

    const uint8_t hello[] = {'H', 'e', 'l', 'l', 'o'};
    out->write(hello, 0, 5);

    std::string s = target.str();
    assert(s == "Hello");
}

static void test_append_and_clear() {
    StringTarget target;
    const uint8_t data[] = {'A', 'B'};
    target.append(data, 2);
    assert(target.str() == "AB");

    target.clear();
    assert(target.str().empty());
}

static void test_charset_and_getters() {
    StringTarget target("");
    assert(target.getCharset() == "UTF-8");

    target.setCharset("UTF-16");
    assert(target.getCharset() == "UTF-16");

    target.setName("buf");
    assert(target.getName() == "buf");

    // URI/URL are default-constructed; just ensure calls compile and return something.
    (void)target.getURI();
    (void)target.getURL();
}

static void test_newWriter_uses_encoding_stream() {
    StringTarget target;
    auto writer = target.newWriter(false);
    // Minimal smoke test: write ASCII bytes (valid UTF-8) and ensure they appear in the buffer.
    const uint8_t data[] = {'X', 'Y', 'Z'};
    writer->write(data, 0, 3);
    writer->flush();

    std::string s = target.str();
    assert(s == "XYZ");
}

int main() {
    test_newOutputStream_and_str();
    test_append_and_clear();
    test_charset_and_getters();
    test_newWriter_uses_encoding_stream();
    std::cout << "All StringTarget tests passed.\n";
    return 0;
}

