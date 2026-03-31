#ifndef UINT8ARRAYOUTPUTSTREAM_H
#define UINT8ARRAYOUTPUTSTREAM_H

#include "OutputStream.hpp"

#include <vector>

class Uint8ArrayOutputStream : public OutputStream {
public:
    Uint8ArrayOutputStream();

    ~Uint8ArrayOutputStream() override = default;
    
    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;

    // Get the written data
    std::vector<uint8_t> data() const { return m_data; }

    // Clear the buffer
    void clear();

    void flush() override;
    void close() override;

private:
    std::vector<uint8_t> m_data;
    bool m_closed;
};

#endif // UINT8ARRAYOUTPUTSTREAM_H
