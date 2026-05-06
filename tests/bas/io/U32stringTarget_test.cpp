// Unit tests for U32stringTarget (UTF-8 path: newOutputStream write then close -> str())

#include "U32stringTarget.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_newOutputStream_utf8() {
    U32stringTarget target;
    auto out = target.newOutputStream(false);
    out->write('H');
    out->write('i');
    out->close();
    const std::u32string& s = target.str();
    assert(s.size() == 2);
    assert(s[0] == U'H' && s[1] == U'i');
}

static void test_newWriter_utf8() {
    U32stringTarget target;
    auto w = target.newWriter(false);
    const uint8_t buf[] = {'x', 'y', 'z'};
    w->write(buf, 0, 3);
    w->flush();
    w->close();
    const std::u32string& s = target.str();
    assert(s.size() == 3);
    assert(s[0] == U'x' && s[2] == U'z');
}

static void test_clear_and_append() {
    U32stringTarget target;
    auto out = target.newOutputStream(false);
    out->write('A');
    out->close();
    assert(target.str().size() == 1);
    target.clear();
    assert(target.str().empty());
    auto out2 = target.newOutputStream(true);
    out2->write('B');
    out2->close();
    assert(target.str().size() == 1 && target.str()[0] == U'B');
}

static void test_charset_getters() {
    U32stringTarget target("");
    assert(target.getCharset() == "UTF-8");
    target.setCharset("UTF-16");
    assert(target.getCharset() == "UTF-16");
    target.setName("u32");
    assert(target.getName() == "u32");
}

int main() {
    test_newOutputStream_utf8();
    test_newWriter_utf8();
    test_clear_and_append();
    test_charset_getters();
    std::cout << "All U32stringTarget tests passed.\n";
    return 0;
}
