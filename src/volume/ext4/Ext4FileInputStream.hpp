#ifndef EXT4FILEINPUTSTREAM_H
#define EXT4FILEINPUTSTREAM_H

#include "../../io/InputStream.hpp"

#include <cstdint>
#include <ios>
#include <string>

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
typedef struct struct_ext2_filsys* ext2_filsys;
typedef struct struct_ext2_file* ext2_file_t;
#endif

class Ext4FileInputStream : public RandomInputStream {
public:
#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
    Ext4FileInputStream(std::string imagePath, uint32_t inode);
#else
    Ext4FileInputStream(std::string imagePath, uint32_t inode);
#endif
    ~Ext4FileInputStream() override;

    int read() override;
    size_t read(uint8_t* buf, size_t off, size_t len) override;
    int64_t skip(int64_t len) override;
    int64_t position() const override;
    bool seek(int64_t offset, std::ios::seekdir dir = std::ios::beg) override;
    void close() override;

private:
    std::string m_imagePath;
    uint32_t m_inode = 0;
    int64_t m_pos = 0;

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
    ext2_filsys m_fs = nullptr;
    ext2_file_t m_file = nullptr;
#endif
};

#endif // EXT4FILEINPUTSTREAM_H
