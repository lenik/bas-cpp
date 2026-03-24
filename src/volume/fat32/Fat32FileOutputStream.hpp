#ifndef FAT32FILEOUTPUTSTREAM_H
#define FAT32FILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"

#include <string>

class Fat32FileOutputStream : public OutputStream {
public:
    Fat32FileOutputStream(std::string imagePath, std::string path, bool append);
    ~Fat32FileOutputStream() override = default;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    std::string m_imagePath;
    std::string m_path;
    bool m_append = false;
};

#endif // FAT32FILEOUTPUTSTREAM_H
