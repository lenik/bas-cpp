#ifndef EXFATFILEOUTPUTSTREAM_H
#define EXFATFILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"

#include <string>

class ExfatFileOutputStream : public OutputStream {
public:
    ExfatFileOutputStream(std::string imagePath, std::string path, bool append);
    ~ExfatFileOutputStream() override = default;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    std::string m_imagePath;
    std::string m_path;
    bool m_append = false;
};

#endif // EXFATFILEOUTPUTSTREAM_H
