// Unit tests for EncodingWriteStream (via a concrete sink that captures bytes)

#include "EncodingWriteStream.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

class SinkEncodingWriteStream : public EncodingWriteStream {
public:
    explicit SinkEncodingWriteStream(std::string_view encoding = "UTF-8")
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

    std::vector<uint8_t> bytes() const { return m_bytes; }
    std::string str() const {
        return std::string(reinterpret_cast<const char*>(m_bytes.data()), m_bytes.size());
    }

private:
    std::vector<uint8_t> m_bytes;
    bool m_closed;
};

static void test_getEncoding() {
    SinkEncodingWriteStream s("UTF-8");
    assert(s.getEncoding() == "UTF-8");
    SinkEncodingWriteStream s2("");
    assert(s2.getEncoding() == "UTF-8");
    SinkEncodingWriteStream s3("ISO-8859-1");
    assert(s3.getEncoding() == "ISO-8859-1");
}

static void test_write_byte() {
    SinkEncodingWriteStream s;
    assert(s.write(0x41));
    assert(s.bytes().size() == 1u);
    assert(s.bytes()[0] == 0x41);
    assert(s.write(0xFF));
    assert(s.bytes().size() == 2u);
    assert(s.bytes()[1] == 0xFF);
}

static void test_writeChar_ascii() {
    SinkEncodingWriteStream s;
    assert(s.writeChar(U'A'));
    assert(s.str() == "A");
    assert(s.writeChar(U'B'));
    assert(s.str() == "AB");
}

static void test_writeChar_utf8() {
    SinkEncodingWriteStream s;
    assert(s.writeChar(U'\u00E9'));  // é
    assert(s.bytes().size() == 2u);
    assert(static_cast<unsigned char>(s.bytes()[0]) == 0xC3u);
    assert(static_cast<unsigned char>(s.bytes()[1]) == 0xA9u);
}

static void test_write_buf() {
    SinkEncodingWriteStream s;
    const uint8_t buf[] = { 'x', 'y', 'z' };
    size_t n = s.write(buf, 0, 3);
    assert(n == 3u);
    assert(s.str() == "xyz");
    n = s.write(buf, 1, 1);
    assert(n == 1u);
    assert(s.str() == "xyzy");
}

static void test_write_string_view() {
    SinkEncodingWriteStream s;
    assert(s.write(std::string_view("hi")));
    assert(s.str() == "hi");
    assert(s.write(std::string_view("")));
    assert(s.str() == "hi");
}

static void test_writeln() {
    SinkEncodingWriteStream s;
    assert(s.writeln(std::string_view("line")));
    assert(s.str() == "line\n");
    assert(s.writeln(std::string_view("")));
    assert(s.str() == "line\n\n");
}

static void test_malform_replace() {
    SinkEncodingWriteStream s;
    assert(s.malformAction() == EncodingWriteStream::MalformAction::Replace);
    assert(s.malformReplacement() == '?');
    // Invalid code point (> 0x10FFFF) -> utf8EncodeCodePoint returns empty; Replace outputs '?'
    assert(s.writeChar(static_cast<char32_t>(0x110000)));
    assert(s.bytes().size() == 1u && s.bytes()[0] == '?');
    s.setMalformReplacement('!');
    assert(s.malformReplacement() == '!');
}

static void test_malform_skip() {
    SinkEncodingWriteStream s;
    s.setMalformAction(EncodingWriteStream::MalformAction::Skip);
    assert(s.writeChar(static_cast<char32_t>(0x110000)));  // invalid, skip -> true, no bytes
    assert(s.bytes().empty());
}

static void test_malform_error() {
    SinkEncodingWriteStream s;
    s.setMalformAction(EncodingWriteStream::MalformAction::Error);
    assert(!s.writeChar(static_cast<char32_t>(0x110000)));
    assert(s.bytes().empty());
}

static void test_close() {
    SinkEncodingWriteStream s;
    s.write('a');
    s.close();
    assert(s.write('b') == false);
    assert(s.write(nullptr, 0, 0) == 0u);
    assert(s.bytes().size() == 1u);
}

int main() {
    test_getEncoding();
    test_write_byte();
    test_writeChar_ascii();
    test_writeChar_utf8();
    test_write_buf();
    test_write_string_view();
    test_writeln();
    test_malform_replace();
    test_malform_skip();
    test_malform_error();
    test_close();
    std::cout << "All EncodingWriteStream tests passed.\n";
    return 0;
}
