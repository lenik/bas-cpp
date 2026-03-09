#ifndef U32STRING_READER_H
#define U32STRING_READER_H

#include "Reader.hpp"

#include <string>

/** Reader over UTF-32 string (view). Semantic character-based; reads char32_t code points. Does not own the data. */
class U32stringReader : public RandomReader {
public:
    explicit U32stringReader(const std::u32string& data);
    explicit U32stringReader(const std::string& data);

    ~U32stringReader() override = default;

    int readChar() override;
    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    std::string readLineChopped() override;
    int64_t skipChars(int64_t len) override;
    void close() override;

    int64_t charPosition() const override;
    bool seekChars(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

private:
    const char32_t* m_data;
    size_t m_len;
    size_t m_pos;
};

#endif // U32STRING_READER_H
