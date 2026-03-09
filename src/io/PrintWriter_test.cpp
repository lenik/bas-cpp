// Unit tests for PrintWriter

#include "PrintWriter.hpp"
#include "StringWriter.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_delegate_write_and_println() {
    StringWriter sw;
    PrintWriter pw(&sw);

    assert(pw.print("Hello"));
    assert(pw.println());
    assert(pw.println("World"));
    assert(pw.writeChar('!'));

    std::string s = sw.str();
    assert(s == "Hello\nWorld\n!");
}

static void test_underlying() {
    StringWriter sw;
    PrintWriter pw(&sw);
    assert(pw.underlying() == &sw);
}

static void test_null_underlying_returns_false() {
    PrintWriter pw(nullptr);
    assert(!pw.write("x"));
    assert(!pw.writeChar('a'));
    assert(!pw.writeln());
    assert(!pw.writeln("y"));
    pw.flush();
    pw.close();
}

int main() {
    test_delegate_write_and_println();
    test_underlying();
    test_null_underlying_returns_false();
    std::cout << "All PrintWriter tests passed.\n";
    return 0;
}
