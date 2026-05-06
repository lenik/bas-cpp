// Unit tests for LocalInputStream

#include "LocalInputStream.hpp"

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

static void test_read_from_file() {
    char path[] = "/tmp/LocalInputStream_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    const char content[] = "hello";
    write(fd, content, sizeof(content) - 1);
    close(fd);

    LocalInputStream in(path);
    assert(in.isOpen());
    assert(in.read() == 'h');
    assert(in.read() == 'e');
    assert(in.read() == 'l');
    assert(in.read() == 'l');
    assert(in.read() == 'o');
    assert(in.read() == -1);
    in.close();
    assert(!in.isOpen());
    unlink(path);
}

static void test_seek_and_position() {
    char path[] = "/tmp/LocalInputStream_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    const char content[] = "ABCD";
    write(fd, content, 4);
    close(fd);

    LocalInputStream in(path);
    assert(in.position() == 0);
    assert(in.read() == 'A');
    assert(in.position() == 1);
    assert(in.seek(0, std::ios::beg));
    assert(in.position() == 0);
    assert(in.read() == 'A');
    assert(in.seek(2, std::ios::beg));
    assert(in.read() == 'C');
    in.close();
    unlink(path);
}

int main() {
    test_read_from_file();
    test_seek_and_position();
    std::cout << "All LocalInputStream tests passed.\n";
    return 0;
}
