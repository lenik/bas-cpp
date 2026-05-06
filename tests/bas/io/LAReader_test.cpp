// Unit tests for LAReader (via a concrete reader that wraps u32string)

#include "LAReader.hpp"

#include "../util/unicode.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

/** LAReader that reads from a UTF-32 string. */
class LAReaderFromU32 : public LAReader {
public:
    explicit LAReaderFromU32(std::u32string data, size_t lookAhead = 4)
        : LAReader(lookAhead)
        , m_data(std::move(data))
        , m_pos(0)
    {}

    int readCharUnbuffered() override {
        if (m_pos >= m_data.size()) return -1;
        return static_cast<int>(m_data[m_pos++]);
    }

    size_t readCharsUnbuffered(char* buf, size_t off, size_t len) override {
        if (!buf || len == 0) return 0;
        size_t written = 0;
        while (written < len && m_pos < m_data.size()) {
            std::string utf8;
            unicode::utf8EncodeCodePoint(static_cast<int>(m_data[m_pos++]), utf8);
            size_t n = std::min(utf8.size(), len - written);
            std::memcpy(buf + off + written, utf8.data(), n);
            written += n;
        }
        return written;
    }

    size_t skipCharsUnbuffered(size_t n) override {
        size_t s = std::min(n, m_data.size() - m_pos);
        m_pos += s;
        return s;
    }

    int64_t charPositionUnbuffered() const override {
        return static_cast<int64_t>(m_pos);
    }

    bool seekChars(int64_t offset, std::ios::seekdir dir) override {
        int64_t newPos = 0;
        switch (dir) {
        case std::ios::beg: newPos = offset; break;
        case std::ios::cur: newPos = static_cast<int64_t>(m_pos) + offset; break;
        case std::ios::end: newPos = static_cast<int64_t>(m_data.size()) + offset; break;
        default: return false;
        }
        if (newPos < 0 || newPos > static_cast<int64_t>(m_data.size())) return false;
        m_pos = static_cast<size_t>(newPos);
        return true;
    }

    void close() override { m_pos = m_data.size(); }

private:
    std::u32string m_data;
    size_t m_pos;
};

static void test_readChar_and_lookAhead() {
    LAReaderFromU32 r(U"AB", 4);
    assert(r.lookAheadChar() == 'A');
    assert(r.readChar() == 'A');
    assert(r.lookAheadChar() == 'B');
    assert(r.readChar() == 'B');
    assert(r.readChar() == -1);
    assert(r.lookAheadChar() == -1);
}

static void test_rejectChars() {
    LAReaderFromU32 r(U"xy", 4);
    assert(r.readChar() == 'x');
    assert(r.rejectChars(1));
    assert(r.readChar() == 'x');
    assert(r.readChar() == 'y');
    assert(r.readChar() == -1);
}

static void test_readLine() {
    LAReaderFromU32 r(U"a\nb\n", 4);
    assert(r.readLine() == "a\n");
    assert(r.readLine() == "b\n");
    assert(r.readLine().empty());
}

static void test_readCharsUntilEOF() {
    LAReaderFromU32 r(U"hi", 4);
    assert(r.readCharsUntilEOF() == "hi");
    assert(r.readCharsUntilEOF().empty());
}

int main() {
    test_readChar_and_lookAhead();
    test_rejectChars();
    test_readLine();
    test_readCharsUntilEOF();
    std::cout << "All LAReader tests passed.\n";
    return 0;
}
