// Unit tests for DataProtocol.
// Build: requires linking NetProtocol and full io chain (e.g. meson); not in src/tests.mf.

#include "DataProtocol.hpp"
#include "URL.hpp"

#include <cassert>
#include <iostream>

static void test_getProtocolName() {
    DataProtocol p;
    assert(p.getProtocolName() == "data");
}

static void test_newInputStream() {
    DataProtocol p;
    URL url("data:text/plain,hello");
    auto in = p.newInputStream(url);
    assert(in);
    assert(in->read() == 'h');
    assert(in->read() == 'e');
    in->close();
}

static void test_newOutputStream_returns_null() {
    DataProtocol p;
    URL url("data:text/plain,foo");
    auto out = p.newOutputStream(url, false);
    assert(!out);  // data: is read-only
}

static void test_newInputStream_invalid_scheme_returns_null() {
    DataProtocol p;
    URL url("http://example.com/");
    auto in = p.newInputStream(url);
    assert(!in);
}

int main() {
    test_getProtocolName();
    test_newInputStream();
    test_newOutputStream_returns_null();
    test_newInputStream_invalid_scheme_returns_null();
    std::cout << "All DataProtocol tests passed.\n";
    return 0;
}
