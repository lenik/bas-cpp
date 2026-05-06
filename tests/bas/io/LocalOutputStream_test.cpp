// Unit tests for LocalOutputStream

#include "LocalOutputStream.hpp"

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

static void test_write_and_read_back() {
    char path[] = "/tmp/LocalOutputStream_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    std::string pathStr(path);

    {
        LocalOutputStream out(pathStr);
        assert(out.isOpen());
        assert(out.write('A'));
        assert(out.write('B'));
        const uint8_t buf[] = {'C', 'D'};
        assert(out.write(buf, 0, 2) == 2);
        out.flush();
        out.close();
        assert(!out.isOpen());
    }
    {
        std::ifstream in(pathStr, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        assert(content == "ABCD");
    }
    unlink(path);
}

static void test_append() {
    char path[] = "/tmp/LocalOutputStream_test_XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    write(fd, "x", 1);
    close(fd);
    std::string pathStr(path);

    {
        LocalOutputStream out(pathStr, true);  // append
        assert(out.isOpen());
        assert(out.write('y'));
        out.close();
    }
    {
        std::ifstream in(pathStr, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        assert(content == "xy");
    }
    unlink(path);
}

int main() {
    test_write_and_read_back();
    test_append();
    std::cout << "All LocalOutputStream tests passed.\n";
    return 0;
}
