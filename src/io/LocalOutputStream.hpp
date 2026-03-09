#ifndef LOCALOUTPUTSTREAM_H
#define LOCALOUTPUTSTREAM_H

#include "OutputStream.hpp"

#include <fstream>

class LocalOutputStream : public OutputStream {
public:
    explicit LocalOutputStream(const std::string& path, bool append = false);
    ~LocalOutputStream();

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;

    void flush() override;

    void close() override;
    bool isOpen() const { return m_file.is_open(); }

private:
    std::ofstream m_file;
};

#endif // LOCALOUTPUTSTREAM_H

