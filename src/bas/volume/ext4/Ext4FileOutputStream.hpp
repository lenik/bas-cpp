#ifndef EXT4FILEOUTPUTSTREAM_H
#define EXT4FILEOUTPUTSTREAM_H

#include "../../io/OutputStream.hpp"

#include <string>
#include <vector>
#include <cstdint>

class Ext4Volume;

class Ext4FileOutputStream : public OutputStream {
public:
    Ext4FileOutputStream(Ext4Volume* volume, std::string path, bool append);
    ~Ext4FileOutputStream() override;

    bool write(int byte) override;
    size_t write(const uint8_t* buf, size_t off, size_t len) override;
    void flush() override;
    void close() override;

private:
    void ensureOpen();
    void writeBufferChunk();
    void finalizeAndClose(bool forceFinalize);

    Ext4Volume* m_volume = nullptr;
    std::string m_path;
    bool m_append = false;

    bool m_fileReady = false;
    bool m_needLink = false;
    uint32_t m_parentIno = 0;
    std::string m_baseName;
    void* m_fs = nullptr;   // ext2_filsys (type is included in .cpp only)
    void* m_file = nullptr; // ext2_file_t (type is included in .cpp only)
    uint32_t m_ino = 0;

    size_t m_writePos = 0; // current logical file offset in bytes
    std::vector<uint8_t> m_buffer;
    size_t m_bufferPos = 0;
    bool m_closed = false;
    bool m_open = false;
};

#endif // EXT4FILEOUTPUTSTREAM_H
