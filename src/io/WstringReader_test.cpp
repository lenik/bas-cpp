// Unit tests for WstringReader

#include "WstringReader.hpp"

#include <cassert>
#include <iostream>
#include <string>

static void test_construct_and_readChar() {
    std::wstring w = L"AB";
    WstringReader r(w);
    assert(r.readChar() == L'A');
    assert(r.readChar() == L'B');
    assert(r.readChar() == -1);
}

static void test_readChars() {
    std::wstring w = L"abc";
    WstringReader r(w);
    assert(r.readChars(2) == "ab");
    assert(r.readChars(1) == "c");
    assert(r.readChars(1).empty());
}

static void test_readCharsUntilEOF() {
    std::wstring w = L"xyz";
    WstringReader r(w);
    assert(r.readCharsUntilEOF() == "xyz");
    assert(r.readCharsUntilEOF().empty());
}

static void test_readLine_and_chopped() {
    std::wstring w = L"L1\nL2\n";
    WstringReader r(w);
    assert(r.readLineChopped() == "L1\n");
    assert(r.readLineChopped() == "L2\n");
    assert(r.readLineChopped().empty());
}

static void test_readLine_no_trailing_newline() {
    std::wstring w = L"only";
    WstringReader r(w);
    assert(r.readLine() == "only");
    assert(r.readLine().empty());
}

static void test_skipChars() {
    std::wstring w = L"12345";
    WstringReader r(w);
    assert(r.skipChars(2) == 2);
    assert(r.readChar() == L'3');
}

static void test_close() {
    std::wstring w = L"x";
    WstringReader r(w);
    r.close();
    assert(r.readChar() == -1);
}

static void test_empty_string() {
    std::wstring w = L"";
    WstringReader r(w);
    assert(r.readChar() == -1);
    assert(r.readChars(1).empty());
    assert(r.readLine().empty());
}

#if WCHAR_MAX > 0xFFFF
// 32-bit wchar_t: single code unit per character
static void test_utf32_style_wide() {
    std::wstring w = L"\u00E9";  // U+00E9 (é)
    WstringReader r(w);
    std::string s = r.readCharsUntilEOF();
    assert(s.size() == 2u);
    assert(static_cast<unsigned char>(s[0]) == 0xC3u);
    assert(static_cast<unsigned char>(s[1]) == 0xA9u);
}
#endif

int main() {
    test_construct_and_readChar();
    test_readChars();
    test_readCharsUntilEOF();
    test_readLine_and_chopped();
    test_readLine_no_trailing_newline();
    test_skipChars();
    test_close();
    test_empty_string();
#if WCHAR_MAX > 0xFFFF
    test_utf32_style_wide();
#endif
    std::cout << "All WstringReader tests passed.\n";
    return 0;
}
