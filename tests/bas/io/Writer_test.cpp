// Unit tests for Writer (base class)

#include "Writer.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

class TestWriter : public Writer {
public:
    bool writeChar(char32_t codePoint) override {
        out.push_back(static_cast<char32_t>(codePoint));
        return true;
    }

    std::u32string out;
};

} // namespace

static void test_write_simple_utf8() {
    TestWriter w;
    bool ok = w.write("Hello");
    assert(ok);
    assert(w.out.size() == 5);
    assert(w.out[0] == U'H' && w.out[4] == U'o');
}

static void test_writeln_variants() {
    TestWriter w;
    bool ok = w.write("Hi");
    assert(ok);
    assert(w.writeln());
    assert(w.writeln(" there"));

    // Expect: 'H','i','\n',' ','t','h','e','r','e','\n'
    assert(w.out.size() == 10);
    assert(w.out[0] == U'H');
    assert(w.out[2] == U'\n');
    assert(w.out[3] == U' ');
    assert(w.out.back() == U'\n');
}

static void test_malformed_utf8_fails() {
    TestWriter w;
    // Lone 0xC0 is malformed UTF-8
    std::string bad(1, static_cast<char>(0xC0));
    bool ok = w.write(bad);
    assert(!ok);
    // No code points should have been written
    assert(w.out.empty());
}

int main() {
    test_write_simple_utf8();
    test_writeln_variants();
    test_malformed_utf8_fails();
    std::cout << "All Writer tests passed.\n";
    return 0;
}

