#ifndef EXFATFILEOUTPUTSTREAM_H
#define EXFATFILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"
#include "ExfatVolume.hpp"

#include <string>
#include <vector>

class ExfatFileOutputStream : public OutputStream {
public:
    ExfatFileOutputStream(ExfatVolume* volume, std::string path, bool append);
    ~ExfatFileOutputStream() override;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    void loadExisting();
    void flushAppendOnly();
    void flushRewriteAll();

    ExfatVolume* m_volume = nullptr;
    std::string m_path;
    bool m_append = false;
    bool m_appendOnlyMode = false;
    std::vector<uint32_t> m_clusters;
    ExfatVolume::Dirent m_dirent;
    std::vector<uint8_t> m_fileData;
    size_t m_writePos = 0;
    std::vector<uint8_t> m_buffer = std::vector<uint8_t>(65536);
    size_t m_bufferPos = 0;
    bool m_closed = false;
    bool m_open = true;
};

#endif // EXFATFILEOUTPUTSTREAM_H
