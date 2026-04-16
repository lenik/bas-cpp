#ifndef FAT32FILEINPUTSTREAM_H
#define FAT32FILEINPUTSTREAM_H

#include "../../io/InputStream.hpp"
#include "../BlockDevice.hpp"

#include <cstdint>
#include <ios>
#include <memory>
#include <vector>

class Fat32FileInputStream : public RandomInputStream {
public:
    Fat32FileInputStream(std::shared_ptr<BlockDevice> device, std::vector<uint32_t> clusterChain, uint64_t dataOffset,
                         uint32_t clusterSize, uint64_t fileSize);
    ~Fat32FileInputStream() override;

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    int64_t skip(int64_t len) override;

    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;

    void close() override;
    bool isOpen() const { return static_cast<bool>(m_device); }

private:
    std::shared_ptr<BlockDevice> m_device;
    std::vector<uint32_t> m_chain;
    uint64_t m_dataOffset = 0;
    uint32_t m_clusterSize = 0;
    uint64_t m_fileSize = 0;
    uint64_t m_pos = 0;
    bool readAt(uint64_t pos, uint8_t* dst, size_t len);
};

#endif // FAT32FILEINPUTSTREAM_H
