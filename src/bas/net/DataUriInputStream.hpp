#ifndef DATAURIINPUTSTREAM_H
#define DATAURIINPUTSTREAM_H

#include "../io/InputStream.hpp"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <string>
#include <vector>

/** InputStream that reads from a data: URI (RFC 2397). Decodes base64 when specified. */
class DataUriInputStream : public RandomInputStream {
public:
    /** Parse data: URI or raw payload. If isDataUri then spec is full "data:[;base64],payload". */
    explicit DataUriInputStream(const std::string& spec, bool isDataUri = true);
    /** Direct payload (no base64). */
    DataUriInputStream(std::vector<uint8_t> payload);

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    void close() override;

    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

    bool ok() const { return m_ok; }
    const std::string& mimeType() const { return m_mimeType; }

private:
    void parse(const std::string& spec);

    std::vector<uint8_t> m_data;
    std::string m_mimeType;
    size_t m_position;
    bool m_closed;
    bool m_ok;
};

#endif // DATAURIINPUTSTREAM_H
