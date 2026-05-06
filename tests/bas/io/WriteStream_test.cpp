// Unit tests for IWriteStream interface (tested via EncodingWriteStream)

#include "EncodingWriteStream.hpp"
#include "WriteStream.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class SinkWriteStream : public EncodingWriteStream {
public:
    explicit SinkWriteStream(std::string_view encoding = "UTF-8")
        : EncodingWriteStream(encoding)
        , m_closed(false)
    {}

    size_t _write(const uint8_t* buf, size_t len) override {
        if (m_closed || !buf) return 0;
        for (size_t i = 0; i < len; ++i)
            m_bytes.push_back(buf[i]);
        return len;
    }
    void flush() override {}
    void close() override { m_closed = true; }

    std::string str() const {
        return std::string(reinterpret_cast<const char*>(m_bytes.data()), m_bytes.size());
    }

private:
    std::vector<uint8_t> m_bytes;
    bool m_closed;
};

static void test_interface_getEncoding() {
    SinkWriteStream s("UTF-8");
    IWriteStream* p = &s;
    assert(p->getEncoding() == "UTF-8");
}

static void test_interface_write_and_writeln() {
    SinkWriteStream s;
    IWriteStream* p = &s;
    assert(p->write(std::string_view("a")));
    assert(p->writeln(std::string_view("b")));
    assert(s.str() == "ab\n");
}

static void test_interface_write_byte() {
    SinkWriteStream s;
    IWriteStream* p = &s;
    assert(p->write(0x41));
    assert(p->write(0x42));
    assert(s.str() == "AB");
}

static void test_interface_close() {
    SinkWriteStream s;
    IWriteStream* p = &s;
    p->write('x');
    p->close();
    assert(!p->write('y'));
    assert(s.str() == "x");
}

int main() {
    test_interface_getEncoding();
    test_interface_write_and_writeln();
    test_interface_write_byte();
    test_interface_close();
    std::cout << "All WriteStream tests passed.\n";
    return 0;
}
