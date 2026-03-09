#ifndef CURLINPUTSTREAM_H
#define CURLINPUTSTREAM_H

#include "../io/InputStream.hpp"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <string>
#include <vector>

/** InputStream that reads from a URL via libcurl (GET). Supports http, https, ftp, ftps. */
class CurlInputStream : public RandomInputStream {
public:
    explicit CurlInputStream(const std::string& url);
    ~CurlInputStream();

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    void close() override;

    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

    /** True if the GET request succeeded and we have response body. */
    bool ok() const { return m_ok; }

    /** HTTP response code (0 if not HTTP or error). */
    long responseCode() const { return m_responseCode; }

private:
    void perform();

    std::string m_url;
    std::vector<uint8_t> m_data;
    size_t m_position;
    bool m_closed;
    bool m_ok;
    long m_responseCode;
};

#endif // CURLINPUTSTREAM_H
