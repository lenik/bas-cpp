#include "InputStream.hpp"

#include <cstdint>
#include <vector>

size_t InputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (!buf || len == 0) return 0;
    size_t n = 0;
    while (n < len) {
        int b = read();
        if (b == -1) break;
        buf[off + n] = static_cast<uint8_t>(b);
        ++n;
    }
    return n;
}

std::vector<uint8_t> InputStream::readBytes(size_t len) {
    std::vector<uint8_t> out;
    if (len == 0) return out;
    out.reserve(len);
    while (out.size() < len) {
        int b = read();
        if (b == -1) break;
        out.push_back(static_cast<uint8_t>(b));
    }
    return out;
}

std::vector<uint8_t> InputStream::readBytesUntilEOF() {
    std::vector<uint8_t> out;
    while (true) {
        int b = read();
        if (b == -1) break;
        out.push_back(static_cast<uint8_t>(b));
    }
    return out;
}

std::vector<uint8_t> InputStream::readRawLine() {
    std::vector<uint8_t> line;
    while (true) {
        int b = read();
        if (b == -1) break;
        line.push_back(static_cast<uint8_t>(b));
        if (b == '\n') break;
    }
    return line;
}

std::vector<uint8_t> InputStream::readRawLineChopped() {
    std::vector<uint8_t> line = readRawLine();
    if (!line.empty() && line.back() == '\n') {
        line.pop_back();
        if (!line.empty() && line.back() == '\r') line.pop_back();
    }
    return line;
}

int64_t InputStream::skip(int64_t len) {
    if (len <= 0) return 0;
    int64_t n = 0;
    while (n < len) {
        if (read() == -1) break;
        ++n;
    }
    return n;
}
