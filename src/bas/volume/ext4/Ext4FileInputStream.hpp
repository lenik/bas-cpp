#ifndef EXT4FILEINPUTSTREAM_H
#define EXT4FILEINPUTSTREAM_H

#include "../../io/InputStream.hpp"

#include <cstdint>
#include <ios>
#include <memory>
#include <string>

#include <ext2fs/ext2fs.h>

#include "../BlockDevice.hpp"

class Ext4FileInputStream : public RandomInputStream {
public:
    Ext4FileInputStream(std::shared_ptr<BlockDevice> device, uint32_t inode);
    ~Ext4FileInputStream() override;

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    int64_t skip(int64_t len) override;
    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;
    void close() override;

private:
    std::shared_ptr<BlockDevice> m_device;
    std::string m_deviceId;
    uint32_t m_inode = 0;
    int64_t m_pos = 0;

    ext2_filsys m_fs = nullptr;
    ext2_file_t m_file = nullptr;
};

#endif // EXT4FILEINPUTSTREAM_H
