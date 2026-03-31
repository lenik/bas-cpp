#ifndef ENCODINGWRITESTREAMTOOUTPUTSTREAM_H
#define ENCODINGWRITESTREAMTOOUTPUTSTREAM_H

#include "EncodingWriteStream.hpp"
#include "OutputStream.hpp"

#include <memory>

/**
 * EncodingWriteStream that writes to an OutputStream. Owns the underlying stream.
 */
class EncodingWriteStreamToOutputStream : public EncodingWriteStream {
public:
    EncodingWriteStreamToOutputStream(std::unique_ptr<OutputStream> out,
                                      std::string_view encoding = "UTF-8");
    ~EncodingWriteStreamToOutputStream() override = default;

    void flush() override;
    void close() override;

protected:
    size_t _write(const uint8_t* buf, size_t len) override;

private:
    std::unique_ptr<OutputStream> m_out;
    bool m_closed;
};

#endif // ENCODINGWRITESTREAMTOOUTPUTSTREAM_H
