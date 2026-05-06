#include "Ext4FileOutputStream.hpp"

#include "Ext4IoManager.hpp"
#include "Ext4Volume.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <limits>
#include <vector>

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

Ext4FileOutputStream::Ext4FileOutputStream(Ext4Volume* volume, std::string path, bool append)
    : m_volume(volume)
    , m_path(std::move(path))
    , m_append(append)
    , m_buffer(65536)  // 64KB buffer
    , m_bufferPos(0)
    , m_closed(false)
    , m_open(true)
{
    if (!m_volume || m_path.empty()) {
        throw IOException("Ext4FileOutputStream", "", "Empty path");
    }

    m_fileReady = false;
    m_needLink = false;
    m_ino = 0;
    m_writePos = 0;
    m_parentIno = 0;
    m_baseName.clear();

    const bool exists = m_volume->exists(m_path);
    Ext4Volume::Inode node;
    if (exists) {
        if (!m_volume->resolveNode(m_path, &node) || node.isDirectory) {
            throw IOException("Ext4FileOutputStream", m_path, "Append target is invalid");
        }
        m_ino = node.ino;
        m_writePos = m_append ? static_cast<size_t>(node.size) : 0;
        m_needLink = false;
    } else {
        // Treat append-on-missing as create.
        m_needLink = true;
        m_parentIno = m_volume->resolveParentInode(m_path);
        m_baseName = m_volume->getBaseName(m_path);
        if (m_baseName.empty()) {
            throw IOException("Ext4FileOutputStream", m_path, "Invalid file name");
        }
        m_writePos = 0;
        m_ino = 0;
    }
}

Ext4FileOutputStream::~Ext4FileOutputStream() {
    try {
        close();
    } catch (...) {
        // Ignore exceptions in destructor
    }
}

bool Ext4FileOutputStream::write(int byte) {
    if (m_closed || !m_open) {
        return false;
    }
    if (m_bufferPos >= m_buffer.size()) {
        flush();
    }
    m_buffer[m_bufferPos++] = static_cast<uint8_t>(byte);
    return true;
}

size_t Ext4FileOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !m_open || !buf || len == 0) {
        return 0;
    }
    
    size_t written = 0;
    while (written < len) {
        if (m_bufferPos >= m_buffer.size()) {
            flush();
            if (m_bufferPos >= m_buffer.size()) break;
        }
        
        size_t toCopy = std::min(len - written, m_buffer.size() - m_bufferPos);
        memcpy(m_buffer.data() + m_bufferPos, buf + off + written, toCopy);
        m_bufferPos += toCopy;
        written += toCopy;
    }
    
    return written;
}

void Ext4FileOutputStream::ensureOpen() {
    if (m_fileReady) return;
    if (!m_volume || m_path.empty()) {
        throw IOException("Ext4FileOutputStream", m_path, "Invalid stream state");
    }

    if (m_needLink && (m_parentIno == 0 || m_baseName.empty())) {
        throw IOException("Ext4FileOutputStream", m_path, "Missing parent/filename for new file");
    }
    if (!m_needLink && m_ino == 0) {
        throw IOException("Ext4FileOutputStream", m_path, "Missing inode for existing file");
    }

    ext2_filsys fs = nullptr;
    int rc = ext4io::openFsFromBlockDevice(m_volume->m_device, EXT2_FLAG_64BITS | EXT2_FLAG_RW, &fs);
    if (rc) {
        throw IOException("Ext4FileOutputStream", m_volume->m_device->uri(), "ext2fs_open failed");
    }

    rc = ext2fs_read_bitmaps(fs);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("Ext4FileOutputStream", m_volume->m_device->uri(), "read_bitmaps failed");
    }

    if (m_needLink) {
        rc = ext2fs_new_inode(fs, m_parentIno, 0100644, nullptr, &m_ino);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("Ext4FileOutputStream", m_path, "Failed allocating inode");
        }
        struct ext2_inode inode{};
        memset(&inode, 0, sizeof(inode));
        inode.i_mode = 0100644;
        inode.i_uid = static_cast<uint32_t>(m_volume->m_contextUid);
        inode.i_gid = static_cast<uint32_t>(m_volume->m_contextGid);
        const uint32_t now = static_cast<uint32_t>(time(nullptr));
        inode.i_atime = inode.i_mtime = inode.i_ctime = now;
        rc = ext2fs_write_inode(fs, m_ino, &inode);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("Ext4FileOutputStream", m_path, "Failed initializing inode");
        }
    }

    unsigned int openFlags = EXT2_FILE_WRITE;
    if (m_needLink) openFlags |= EXT2_FILE_CREATE;
    ext2_file_t file = nullptr;
    rc = ext2fs_file_open(fs, m_ino, openFlags, &file);
    if (rc) {
        ext2fs_close(fs);
        m_file = nullptr;
        throw IOException("Ext4FileOutputStream", m_path, "Failed opening inode file");
    }
    m_file = file;

    if (!m_append) {
        ext2_file_t ofile = static_cast<ext2_file_t>(m_file);
        rc = ext2fs_file_set_size2(ofile, 0);
        if (rc) {
            ext2fs_file_close(ofile);
            ext2fs_close(fs);
            m_file = nullptr;
            throw IOException("Ext4FileOutputStream", m_path, "Failed truncate to 0");
        }
        rc = ext2fs_file_llseek(ofile, 0, 0, nullptr);
        if (rc) {
            ext2fs_file_close(ofile);
            ext2fs_close(fs);
            m_file = nullptr;
            throw IOException("Ext4FileOutputStream", m_path, "Failed seek to 0");
        }
    } else {
        ext2_file_t ofile = static_cast<ext2_file_t>(m_file);
        rc = ext2fs_file_llseek(ofile, static_cast<ext2_off64_t>(m_writePos), 0, nullptr);
        if (rc) {
            ext2fs_file_close(ofile);
            ext2fs_close(fs);
            m_file = nullptr;
            throw IOException("Ext4FileOutputStream", m_path, "Failed seek to EOF");
        }
    }

    m_fs = fs;
    m_fileReady = true;
}

void Ext4FileOutputStream::writeBufferChunk() {
    if (m_bufferPos == 0) return;
    ensureOpen();

    size_t off = 0;
    while (off < m_bufferPos) {
        const size_t n = std::min<size_t>(m_bufferPos - off, static_cast<size_t>(std::numeric_limits<unsigned int>::max()));
        unsigned int written = 0;
        ext2_file_t file = static_cast<ext2_file_t>(m_file);
        int rc = ext2fs_file_write(file, m_buffer.data() + off, static_cast<unsigned int>(n), &written);
        if (rc || written != n) {
            throw IOException("Ext4FileOutputStream", m_path, "Failed writing ext4 data chunk");
        }
        off += written;
    }
    m_writePos += m_bufferPos;
    m_bufferPos = 0;
}

void Ext4FileOutputStream::finalizeAndClose(bool /*forceFinalize*/) {
    if (m_closed) return;

    if (!m_fileReady) {
        // Create empty file if needed.
        ensureOpen();
    }

    if (m_bufferPos > 0) {
        writeBufferChunk();
    }

    if (m_fileReady && m_file) {
        ext2_file_t file = static_cast<ext2_file_t>(m_file);
        int rc = ext2fs_file_set_size2(file, static_cast<ext2_off64_t>(m_writePos));
        if (rc) {
            throw IOException("Ext4FileOutputStream", m_path, "Failed setting final file size");
        }
    }

    if (m_fileReady && m_fs) {
        ext2_filsys fs = static_cast<ext2_filsys>(m_fs);
        int rc = ext2fs_flush(fs);
        if (rc) {
            throw IOException("Ext4FileOutputStream", m_path, "Failed flushing ext4 writes");
        }

        if (m_needLink) {
            rc = ext2fs_link(fs, m_parentIno, m_baseName.c_str(), m_ino, 0100644);
            if (rc == EXT2_ET_DIR_NO_SPACE) {
                rc = ext2fs_expand_dir(fs, m_parentIno);
                if (!rc) {
                    rc = ext2fs_link(fs, m_parentIno, m_baseName.c_str(), m_ino, 0100644);
                }
            }
            if (rc) {
                throw IOException("Ext4FileOutputStream", m_path, "Failed creating directory entry");
            }
            m_needLink = false;
        }
    }

    if (m_fileReady && m_file) {
        ext2_file_t file = static_cast<ext2_file_t>(m_file);
        ext2fs_file_close(file);
    }
    if (m_fileReady && m_fs) {
        ext2_filsys fs = static_cast<ext2_filsys>(m_fs);
        ext2fs_close(fs);
    }
    m_file = nullptr;
    m_fs = nullptr;
    m_fileReady = false;

    if (m_volume) {
        m_volume->invalidateCache(m_path);
    }
}

void Ext4FileOutputStream::flush() {
    if (m_closed || !m_open) return;
    if (m_bufferPos > 0) {
        writeBufferChunk();
        if (m_fileReady && m_fs) {
            ext2_filsys fs = static_cast<ext2_filsys>(m_fs);
            int rc = ext2fs_flush(fs);
            if (rc) {
                throw IOException("Ext4FileOutputStream", m_path, "Failed flush ext4 data");
            }
        }
    }
}

void Ext4FileOutputStream::close() {
    if (!m_closed && m_open) {
        try {
            finalizeAndClose(true);
            m_bufferPos = 0;
        } catch (const IOException&) {
            m_open = false;
            throw;
        }
        m_closed = true;
        m_open = false;
    }
}
