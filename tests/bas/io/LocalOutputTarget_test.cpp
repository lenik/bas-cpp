// Unit tests for LocalOutputTarget

#include "LocalOutputTarget.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

static void test_getters_and_newOutputStream() {
    char path[] = "/tmp/LocalOutputTarget_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    std::string pathStr(path);

    LocalOutputTarget target(pathStr, "UTF-8");
    assert(target.getName() == pathStr);
    assert(target.getCharset() == "UTF-8");

    URI uri = target.getURI();
    assert(uri.scheme().value_or("") == "file");
    URL url = target.getURL();
    assert(!url.empty());

    auto out = target.newOutputStream(false);
    assert(out);
    assert(out->write('X'));
    out->close();

    std::ifstream in(pathStr);
    char c;
    assert(in.get(c) && c == 'X');
    in.close();
    unlink(path);
}

int main() {
    test_getters_and_newOutputStream();
    std::cout << "All LocalOutputTarget tests passed.\n";
    return 0;
}
