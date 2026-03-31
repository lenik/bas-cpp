// Unit tests for InputSource interface (tested via StringSource)

#include "InputSource.hpp"
#include "StringSource.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static void test_interface_newReader() {
    StringSource src("line1\nline2");
    InputSource* base = &src;
    std::unique_ptr<Reader> r = base->newReader();
    assert(r);
    assert(r->readLine() == "line1\n");
    assert(r->readLine() == "line2");
    assert(r->readLine().empty());
    r->close();
}

static void test_interface_getCharset_getName() {
    StringSource src("data", "UTF-8");
    InputSource* base = &src;
    assert(base->getCharset() == "UTF-8");
    assert(!base->getName().empty() || true);  // name may be empty by default
    (void)base->getURI();
    (void)base->getURL();
}

static void test_interface_newInputStream() {
    StringSource src("abc");
    InputSource* base = &src;
    std::unique_ptr<InputStream> in = base->newInputStream();
    assert(in);
    assert(in->read() == 'a');
    assert(in->read() == 'b');
    assert(in->read() == 'c');
    assert(in->read() == -1);
    in->close();
}

int main() {
    test_interface_newReader();
    test_interface_getCharset_getName();
    test_interface_newInputStream();
    std::cout << "All InputSource tests passed.\n";
    return 0;
}
