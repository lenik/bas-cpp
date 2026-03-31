// Unit tests for StringWriter

#include "StringWriter.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_write_and_writeln() {
    StringWriter w;
    bool ok = w.write("Hi");
    assert(ok);
    assert(w.writeChar('!'));
    assert(w.writeln());
    assert(w.writeln("there"));

    std::string s = w.str();
    assert(s == "Hi!\nthere\n");
}

static void test_flush_and_close_noop() {
    StringWriter w;
    w.write("abc");
    w.flush();
    w.close();
    // Buffer should remain intact
    assert(w.str() == "abc");
    // Can still write after close (close is a no-op in this impl)
    w.write("d");
    assert(w.str() == "abcd");
}

int main() {
    test_write_and_writeln();
    test_flush_and_close_noop();
    std::cout << "All StringWriter tests passed.\n";
    return 0;
}

