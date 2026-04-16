#ifndef FAT32FILEOUTPUTSTREAM_H
#define FAT32FILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"
#include "Fat32Volume.hpp"

#include <string>
#include <vector>
#include <cstdint>

class Fat32FileOutputStream : public OutputStream {
public:
    Fat32FileOutputStream(Fat32Volume* volume, std::string path, bool append, uint32_t dirCluster,
                          uint32_t dirIndex);
    ~Fat32FileOutputStream() override;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    void loadExistingFileData();
    void writeFileDataByClusterChain();
    void ensureDirEntry();
    void ensureClustersForSize(uint64_t size, bool reserveHeadroom = false);
    void flushAppendOnly();

    Fat32Volume* m_volume = nullptr;
    std::string m_path;
    bool m_append = false;
    uint32_t m_dirCluster = 0;
    uint32_t m_dirIndex = 0xFFFFFFFFu;
    std::vector<uint32_t> m_clusters;
    Fat32Volume::Dirent m_dirent;
    bool m_appendOnlyMode = false;
    std::vector<uint8_t> m_fileData;
    size_t m_writePos = 0;
    std::vector<uint8_t> m_buffer;
    size_t m_bufferPos = 0;
    bool m_closed = false;
    bool m_open = false;
};

#endif // FAT32FILEOUTPUTSTREAM_H
