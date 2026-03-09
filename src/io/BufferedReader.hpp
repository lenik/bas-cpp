#ifndef BUFFEREDREADER_H
#define BUFFEREDREADER_H

#include "Reader.hpp"

#include <memory>
#include <string>

/**
 * Reader that buffers input from an underlying Reader (Java-style BufferedReader).
 * Reduces calls to the underlying reader by reading in chunks.
 */
class BufferedReader : public Reader {
public:
    explicit BufferedReader(std::unique_ptr<Reader> in);
    virtual ~BufferedReader();

    int readChar() override;
    std::string readChars(size_t numCodePoints) override;
    std::string readCharsUntilEOF() override;
    std::string readLine() override;
    std::string readLineChopped() override;
    int64_t skipChars(int64_t numCodePoints) override;
    void close() override;

private:
    void refill();

    std::unique_ptr<Reader> m_in;
    std::string m_buf;
    size_t m_pos;
    bool m_closed;

    static constexpr size_t BUFFER_CODE_POINTS = 4096;
};

#endif // BUFFEREDREADER_H
