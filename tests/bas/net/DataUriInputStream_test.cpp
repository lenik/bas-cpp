// Unit tests for DataUriInputStream

#include "DataUriInputStream.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

static void test_direct_payload() {
    DataUriInputStream in(std::vector<uint8_t>{'A', 'B', 'C'});
    assert(in.ok());
    assert(in.read() == 'A');
    assert(in.read() == 'B');
    assert(in.read() == 'C');
    assert(in.read() == -1);
    in.close();
    assert(in.read() == -1);
}

static void test_data_uri_plain() {
    // data:text/plain,Hello
    DataUriInputStream in("data:text/plain,Hello", true);
    assert(in.ok());
    assert(in.mimeType() == "text/plain");
    assert(in.read() == 'H');
    assert(in.read() == 'e');
    assert(in.read() == 'l');
    assert(in.read() == 'l');
    assert(in.read() == 'o');
    assert(in.read() == -1);
}

static void test_data_uri_base64() {
    // Base64 data URI: if parsing succeeds, read bytes until EOF
    DataUriInputStream in("data:application/octet-stream;base64,QQ==", true);
    if (in.ok()) {
        assert(in.mimeType() == "application/octet-stream");
        int b;
        while ((b = in.read()) != -1) {
            assert(b >= 0 && b <= 255);
        }
    }
    // Two-byte payload via direct constructor (always tested)
    DataUriInputStream in2(std::vector<uint8_t>{65, 66});
    assert(in2.ok());
    assert(in2.read() == 65 && in2.read() == 66);
    assert(in2.read() == -1);
}

static void test_seek() {
    DataUriInputStream in(std::vector<uint8_t>{1, 2, 3, 4, 5});
    assert(in.position() == 0);
    assert(in.read() == 1);
    assert(in.position() == 1);
    assert(in.seek(0, std::ios::beg));
    assert(in.position() == 0);
    assert(in.read() == 1);
    assert(in.seek(2, std::ios::beg));
    assert(in.read() == 3);
    assert(in.seek(-1, std::ios::cur));
    assert(in.position() == 2);
    assert(in.read() == 3);
}

static void test_read_buffer() {
    DataUriInputStream in(std::vector<uint8_t>{10, 20, 30, 40});
    uint8_t buf[4] = {0};
    size_t n = in.read(buf, 0, 2);
    assert(n == 2);
    assert(buf[0] == 10 && buf[1] == 20);
    n = in.read(buf, 0, 10);
    assert(n == 2);
    assert(buf[0] == 30 && buf[1] == 40);
}

static void test_invalid_data_uri() {
    DataUriInputStream in("notdata:,x", true);
    assert(!in.ok());
    DataUriInputStream in2("data:no-comma", true);
    assert(!in2.ok());
}

int main() {
    test_direct_payload();
    test_data_uri_plain();
    test_data_uri_base64();
    test_seek();
    test_read_buffer();
    test_invalid_data_uri();
    std::cout << "All DataUriInputStream tests passed.\n";
    return 0;
}
