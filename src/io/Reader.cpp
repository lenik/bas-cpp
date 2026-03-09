#include "Reader.hpp"

#include "../util/unicode.hpp"

#include <cstdint>
#include <string>

std::string Reader::readChars(size_t numCodePoints) {
    if (numCodePoints == 0) return {};
    std::string out;
    out.reserve(numCodePoints * 4);  // UTF-8 max 4 bytes per code point
    size_t count = 0;
    while (count < numCodePoints) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
        ++count;
    }
    return out;
}

std::string Reader::readCharsUntilEOF() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

std::string Reader::readLine() {
    std::string out;
    while (true) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
        if (cp == '\n') break;
    }
    return out;
}

std::string Reader::readLineChopped() {
    std::string line = readLine();
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

int64_t Reader::skipChars(int64_t numCodePoints) {
    if (numCodePoints <= 0) return 0;
    int64_t n = 0;
    while (n < numCodePoints) {
        int cp = readChar();
        if (cp < 0) break;
        ++n;
    }
    return n;
}
