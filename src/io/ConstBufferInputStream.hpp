#ifndef CONSTBUFFERINPUTSTREAM_H
#define CONSTBUFFERINPUTSTREAM_H

#include "InputStream.hpp"
#include "Position.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

/** InputStream that reads from a constant byte buffer (e.g. string bytes). Does not own the data. */
class ConstBufferInputStream : public InputStream, virtual public ISeekable {
public:
    /** Reads from the given buffer. */
    ConstBufferInputStream(const uint8_t* data, size_t len);
    /** Reads from string's bytes (reinterpreted as uint8_t). */
    explicit ConstBufferInputStream(std::string_view data);

    ~ConstBufferInputStream() override;

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    void close() override;
    int64_t skip(int64_t len) override;

    int64_t position() const override;
    bool canSeek(std::ios::seekdir dir = std::ios::beg) const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

private:
    const uint8_t* m_data;
    size_t m_len;
    size_t m_position;
    bool m_closed;
};

#endif // CONSTBUFFERINPUTSTREAM_H
