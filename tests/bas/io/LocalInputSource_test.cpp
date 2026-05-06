// Unit tests for LocalInputSource

#include "LocalInputSource.hpp"

#include "../net/URI.hpp"
#include "../net/URL.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

static void test_getters_and_newInputStream() {
    char path[] = "/tmp/LocalInputSource_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    const char content[] = "hello";
    write(fd, content, sizeof(content) - 1);
    close(fd);
    std::string pathStr(path);

    LocalInputSource src(pathStr, "UTF-8");
    assert(src.getName() == pathStr);
    assert(src.getCharset() == "UTF-8");

    URI uri = src.getURI();
    assert(uri.scheme().value_or("") == "file");
    URL url = src.getURL();
    assert(!url.empty());

    auto in = src.newInputStream();
    assert(in);
    assert(in->read() == 'h');
    assert(in->read() == 'e');
    in->close();

    unlink(path);
}

int main() {
    test_getters_and_newInputStream();
    std::cout << "All LocalInputSource tests passed.\n";
    return 0;
}
