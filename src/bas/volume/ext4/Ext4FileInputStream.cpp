#include "Ext4FileInputStream.hpp"

#include "../../io/IOException.hpp"

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

Ext4FileInputStream::Ext4FileInputStream(std::string imagePath, uint32_t inode)
    : m_imagePath(std::move(imagePath)), m_inode(inode) {
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &m_fs);
    if (rc) {
        throw IOException("Ext4FileInputStream", m_imagePath, "ext2fs_open failed");
    }
    rc = ext2fs_file_open(m_fs, m_inode, 0, &m_file);
    if (rc) {
        ext2fs_close(m_fs);
        m_fs = nullptr;
        throw IOException("Ext4FileInputStream", m_imagePath, "ext2fs_file_open failed");
    }
}

Ext4FileInputStream::~Ext4FileInputStream() {
    close();
}

int Ext4FileInputStream::read() {
    uint8_t b = 0;
    return (read(&b, 0, 1) == 1) ? b : -1;
}

size_t Ext4FileInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (!m_file || !buf || len == 0) {
        return 0;
    }
    unsigned int got = 0;
    errcode_t rc = ext2fs_file_read(m_file, buf + off, static_cast<unsigned int>(len), &got);
    if (rc) {
        throw IOException("read", m_imagePath, "ext2fs_file_read failed");
    }
    m_pos += static_cast<int64_t>(got);
    return static_cast<size_t>(got);
}

int64_t Ext4FileInputStream::skip(int64_t len) {
    if (len <= 0) {
        return 0;
    }
    if (!seek(len, std::ios::cur)) {
        return 0;
    }
    return len;
}

int64_t Ext4FileInputStream::position() const {
    return m_pos;
}

bool Ext4FileInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (!m_file) {
        return false;
    }
    uint64_t target = 0;
    if (dir == std::ios::beg) {
        if (offset < 0) {
            return false;
        }
        target = static_cast<uint64_t>(offset);
    } else if (dir == std::ios::cur) {
        if (m_pos + offset < 0) {
            return false;
        }
        target = static_cast<uint64_t>(m_pos + offset);
    } else if (dir == std::ios::end) {
        return false;
    }
    errcode_t rc = ext2fs_file_llseek(m_file, target, 0, nullptr);
    if (rc) {
        return false;
    }
    m_pos = static_cast<int64_t>(target);
    return true;
}

void Ext4FileInputStream::close() {
    if (m_file) {
        ext2fs_file_close(m_file);
        m_file = nullptr;
    }
    if (m_fs) {
        ext2fs_close(m_fs);
        m_fs = nullptr;
    }
}
