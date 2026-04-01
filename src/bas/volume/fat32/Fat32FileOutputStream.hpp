#ifndef FAT32FILEOUTPUTSTREAM_H
#define FAT32FILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"

#include <string>
#include <vector>
#include <cstdint>

class Fat32FileOutputStream : public OutputStream {
public:
    Fat32FileOutputStream(std::string imagePath, std::string path, bool append);
    ~Fat32FileOutputStream() override;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    std::string m_imagePath;
    std::string m_path;
    bool m_append = false;
    std::vector<uint8_t> m_buffer;
    size_t m_bufferPos = 0;
    bool m_closed = false;
    bool m_open = false;
};

#endif // FAT32FILEOUTPUTSTREAM_H
