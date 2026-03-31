#ifndef EXT4FILEOUTPUTSTREAM_H
#define EXT4FILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"

#include <string>

class Ext4FileOutputStream : public OutputStream {
public:
    Ext4FileOutputStream(std::string imagePath, std::string path, bool append);
    ~Ext4FileOutputStream() override = default;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    std::string m_imagePath;
    std::string m_path;
    bool m_append = false;
};

#endif // EXT4FILEOUTPUTSTREAM_H
