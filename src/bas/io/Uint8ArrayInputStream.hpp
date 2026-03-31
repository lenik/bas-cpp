#ifndef UINT8ARRAYINPUTSTREAM_H
#define UINT8ARRAYINPUTSTREAM_H

#include "InputStream.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

/** InputStream over a byte array: full vector, slice, or non-owning pointer+length. */
class Uint8ArrayInputStream : public RandomInputStream {
public:
    static constexpr size_t npos = static_cast<size_t>(-1);

    /** Non-owning slice: reads from data[off .. off+len). len=npos means to end. */
    Uint8ArrayInputStream(const std::vector<uint8_t>& data,
                          size_t off = 0,
                          size_t len = npos);

    /** Owning: takes ownership of \a data (moved). */
    explicit Uint8ArrayInputStream(std::vector<uint8_t>&& data);

    /** Non-owning: reads from data[0..len). Caller must keep data valid. */
    Uint8ArrayInputStream(const uint8_t* data, size_t len);

    virtual ~Uint8ArrayInputStream() = default;
    
    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    void close() override;
    int64_t skip(int64_t len) override;

    int64_t position() const override;
    bool canSeek(std::ios::seekdir dir = std::ios::beg) const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

    /** When constructed from vector (ref or moved), get a copy of the data. */
    std::vector<uint8_t> data() const;

private:
    std::vector<uint8_t> m_owned;
    const uint8_t* m_ptr;
    size_t m_len;
    size_t m_position;
    bool m_closed;
};

#endif // UINT8ARRAYINPUTSTREAM_H
