#ifndef WSTRING_READER_H
#define WSTRING_READER_H

#include "Reader.hpp"

#include <string>

/** Reader over wide string (view). Semantic character-based; reads wchar_t code units. Does not own the data. */
class WstringReader : public Reader {
public:
    explicit WstringReader(const std::wstring& data);

    ~WstringReader() override = default;

    int readChar() override;
    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    std::string readLineChopped() override;
    int64_t skipChars(int64_t len) override;
    void close() override;

private:
    const wchar_t* m_data;
    size_t m_len;
    size_t m_pos;
};

#endif // WSTRING_READER_H
