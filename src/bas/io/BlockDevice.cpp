#include "BlockDevice.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <stdexcept>

// ============================================================================
// FileDevice Implementation
// ============================================================================

FileDevice::FileDevice(const std::string& path) : m_path(path) {
    // Open with O_DIRECT to bypass OS page caching
    // Fall back to regular open if O_DIRECT not supported
    m_fd = open(path.c_str(), O_RDWR | O_SYNC);
    if (m_fd < 0) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    // Get file size
    struct stat st;
    if (fstat(m_fd, &st) != 0) {
        close(m_fd);
        m_fd = -1;
        throw std::runtime_error("Failed to stat file: " + path);
    }
    m_size = static_cast<uint64_t>(st.st_size);
}

FileDevice::~FileDevice() {
    if (m_fd >= 0) {
        flush();
        close(m_fd);
    }
}

bool FileDevice::read(uint64_t offset, uint8_t* dst, size_t len) {
    if (m_fd < 0 || !dst || len == 0 || offset + len > m_size) {
        return false;
    }
    
    off_t pos = pread(m_fd, dst, len, static_cast<off_t>(offset));
    return pos == static_cast<ssize_t>(len);
}

bool FileDevice::write(uint64_t offset, const uint8_t* src, size_t len) {
    if (m_fd < 0 || !src || len == 0 || offset + len > m_size) {
        return false;
    }
    
    off_t pos = pwrite(m_fd, src, len, static_cast<off_t>(offset));
    return pos == static_cast<ssize_t>(len);
}

uint64_t FileDevice::size() const {
    return m_size;
}

bool FileDevice::flush() {
    if (m_fd >= 0) {
        return fsync(m_fd) == 0;
    }
    return false;
}

// ============================================================================
// MemDevice Implementation
// ============================================================================

MemDevice::MemDevice(size_t size) : m_size(size), m_owned(true) {
    m_data = new uint8_t[size];
    memset(m_data, 0, size);
}

MemDevice::MemDevice(const uint8_t* data, size_t size) 
    : m_size(size), m_owned(false) {
    m_data = const_cast<uint8_t*>(data);
}

MemDevice::~MemDevice() {
    if (m_owned && m_data) {
        delete[] m_data;
    }
}

bool MemDevice::read(uint64_t offset, uint8_t* dst, size_t len) {
    if (!m_data || !dst || len == 0 || offset + len > m_size) {
        return false;
    }
    memcpy(dst, m_data + offset, len);
    return true;
}

bool MemDevice::write(uint64_t offset, const uint8_t* src, size_t len) {
    if (!m_data || !src || len == 0 || offset + len > m_size) {
        return false;
    }
    memcpy(m_data + offset, src, len);
    return true;
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<BlockDevice> createFileDevice(const std::string& path) {
    return std::make_shared<FileDevice>(path);
}

std::shared_ptr<BlockDevice> createMemDevice(size_t size) {
    return std::make_shared<MemDevice>(size);
}

std::shared_ptr<BlockDevice> createMemDevice(const uint8_t* data, size_t size) {
    return std::make_shared<MemDevice>(data, size);
}
