// Unit tests for LocalProtocol.
// Build: requires linking NetProtocol and full io chain (e.g. meson); not in src/tests.mf.

#include "LocalProtocol.hpp"
#include "URL.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

static void test_getProtocolName() {
    LocalProtocol p;
    assert(p.getProtocolName() == "file");
}

static void test_newInputStream() {
    char path[] = "/tmp/LocalProtocol_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    const char content[] = "xyz";
    write(fd, content, 3);
    close(fd);

    std::string pathStr(path);
    URL url = URL::local(pathStr);
    LocalProtocol p;
    auto in = p.newInputStream(url);
    assert(in);
    assert(in->read() == 'x');
    assert(in->read() == 'y');
    assert(in->read() == 'z');
    assert(in->read() == -1);
    in->close();
    unlink(path);
}

static void test_newInputStream_wrong_scheme_returns_null() {
    LocalProtocol p;
    URL url("http://example.com/");
    auto in = p.newInputStream(url);
    assert(!in);
}

int main() {
    test_getProtocolName();
    test_newInputStream();
    test_newInputStream_wrong_scheme_returns_null();
    std::cout << "All LocalProtocol tests passed.\n";
    return 0;
}
