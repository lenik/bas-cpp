#ifndef LAINPUTSTREAMFROMSTREAM_H
#define LAINPUTSTREAMFROMSTREAM_H

#include "InputStream.hpp"
#include "LAInputStream.hpp"
#include "Position.hpp"

#include <memory>

/**
 * LAInputStream that wraps an existing InputStream (e.g. for use with DecodingReadStream).
 * Owns the wrapped stream.
 */
class LAInputStreamFromStream : public LAInputStream {
public:
    explicit LAInputStreamFromStream(std::unique_ptr<InputStream> in,
                                     size_t lookAheadCapacity = 4096);

protected:
    int readUnbuffered() override;
    size_t readUnbuffered(uint8_t* buf, size_t off, size_t len) override;
    size_t skipUnbuffered(size_t len) override;
    int64_t positionUnbuffered() const override;
    bool seekUnbuffered(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

public:
    void close() override;

private:
    std::unique_ptr<InputStream> m_in;
};

#endif // LAINPUTSTREAMFROMSTREAM_H
