#include "WstringReader.hpp"

#include "../util/unicode.hpp"

#include <cwchar>

WstringReader::WstringReader(const std::wstring& data)
    : m_data(data.data())
    , m_len(data.size())
    , m_pos(0)
{
}

int WstringReader::readChar() {
    if (m_pos >= m_len) return -1;
    wchar_t cu = m_data[m_pos++];
#if WCHAR_MAX <= 0xFFFF
    if (cu >= 0xD800 && cu <= 0xDBFF && m_pos < m_len) {
        wchar_t low = m_data[m_pos];
        if (low >= 0xDC00 && low <= 0xDFFF) {
            ++m_pos;
            return static_cast<int>(0x10000 + (static_cast<char32_t>(cu - 0xD800) << 10) + (low - 0xDC00));
        }
    }
#endif
    return static_cast<int>(cu);
}

std::string WstringReader::readChars(size_t numCodePoints) {
    std::string out;
    out.reserve(numCodePoints * 4);
    for (size_t n = 0; n < numCodePoints && m_pos < m_len; ++n) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

std::string WstringReader::readCharsUntilEOF() {
    std::string out;
    while (m_pos < m_len) {
        int cp = readChar();
        if (cp < 0) break;
        unicode::utf8EncodeCodePoint(cp, out);
    }
    return out;
}

std::string WstringReader::readLine() {
    size_t start = m_pos;
    while (m_pos < m_len && m_data[m_pos] != L'\n') ++m_pos;
    if (m_pos < m_len) ++m_pos; // include \n
    std::string out;
    out.reserve((m_pos - start) * 4);
    for (size_t i = start; i < m_pos; ) {
        char32_t cp;
#if WCHAR_MAX <= 0xFFFF
        if (m_data[i] >= 0xD800 && m_data[i] <= 0xDBFF && i + 1 < m_pos && m_data[i + 1] >= 0xDC00 && m_data[i + 1] <= 0xDFFF) {
            cp = 0x10000 + (static_cast<char32_t>(m_data[i] - 0xD800) << 10) + (m_data[i + 1] - 0xDC00);
            i += 2;
        } else
#endif
        {
            cp = static_cast<char32_t>(m_data[i++]);
        }
        unicode::utf8EncodeCodePoint(static_cast<int>(cp), out);
    }
    return out;
}

std::string WstringReader::readLineChopped() {
    std::string line = readLine();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

int64_t WstringReader::skipChars(int64_t len) {
    int64_t n = 0;
    while (n < len && m_pos < m_len) {
        wchar_t cu = m_data[m_pos++];
#if WCHAR_MAX <= 0xFFFF
        if (cu >= 0xD800 && cu <= 0xDBFF && m_pos < m_len && m_data[m_pos] >= 0xDC00 && m_data[m_pos] <= 0xDFFF)
            ++m_pos;
#endif
        ++n;
    }
    return n;
}

void WstringReader::close() {
    m_pos = m_len;
}
