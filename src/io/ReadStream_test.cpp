// Unit tests for IReadStream interface (tested via StringReadStream)

#include "ReadStream.hpp"
#include "StringReadStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_interface_getEncoding() {
    StringReadStream s("hello", "UTF-8");
    IReadStream* p = &s;
    assert(p->getEncoding() == "UTF-8");
    p->close();
}

static void test_interface_read() {
    StringReadStream s("AB");
    IReadStream* p = &s;
    assert(p->read() == 'A');
    assert(p->read() == 'B');
    assert(p->read() == -1);
    p->close();
}

static void test_interface_readLine() {
    StringReadStream s("line1\nline2\n");
    IReadStream* p = &s;
    assert(p->readLine() == "line1\n");
    assert(p->readLine() == "line2\n");
    assert(p->readLine().empty());
    p->close();
}

static void test_interface_close() {
    StringReadStream s("x");
    IReadStream* p = &s;
    p->close();
    assert(p->read() == -1);
}

int main() {
    test_interface_getEncoding();
    test_interface_read();
    test_interface_readLine();
    test_interface_close();
    std::cout << "All ReadStream tests passed.\n";
    return 0;
}
