#ifndef CURLOUTPUTSTREAM_H
#define CURLOUTPUTSTREAM_H

#include "../io/OutputStream.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/** OutputStream that uploads to a URL via libcurl (PUT). Supports http, https, ftp, ftps. */
class CurlOutputStream : public OutputStream {
public:
    explicit CurlOutputStream(const std::string& url, bool append = false);
    ~CurlOutputStream();

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

    /** True if the PUT request succeeded. */
    bool ok() const { return m_ok; }

    /** HTTP response code (0 if not HTTP or error). */
    long responseCode() const { return m_responseCode; }

private:
    void performPut();

    std::string m_url;
    std::vector<uint8_t> m_buffer;
    bool m_closed;
    bool m_ok;
    long m_responseCode;
    bool m_append;
};

#endif // CURLOUTPUTSTREAM_H
