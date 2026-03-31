#ifndef STRING_READER_H
#define STRING_READER_H

#include "Reader.hpp"

#include <string_view>

/** Reader over UTF-8 string (view). Does not own the data; view must outlive the reader. */
class StringReader : public Reader {
public:
    explicit StringReader(std::string_view data);

    int readChar() override;
    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    std::string readLineChopped() override;
    int64_t skipChars(int64_t len) override;
    void close() override;

private:
    const char* m_data;
    size_t m_len;
    size_t m_pos;
};

#endif // STRING_READER_H
