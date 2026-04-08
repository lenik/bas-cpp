#include "FileDevice.hpp"

#include "../../util/urls.hpp"

#include <stdexcept>

FileDevice::FileDevice(const std::string& path, uint64_t offset, uint64_t length, bool read_only,
                       bool cached)
    : m_path(path), m_offset(offset), m_size(0), m_read_only(read_only), m_cached(cached) {
    // Open file
    m_fd = open(path.c_str(), O_RDWR);
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

    // Apply offset and length
    if (m_offset >= m_size) {
        close(m_fd);
        m_fd = -1;
        throw std::runtime_error("Offset beyond file size");
    }

    if (length > 0 && length < m_size - m_offset) {
        m_size = length;
    }

    // Preload if requested
    if (m_cached) {
        m_data.resize(m_size);
        if (pread(m_fd, m_data.data(), m_size, m_offset) != static_cast<ssize_t>(m_size)) {
            close(m_fd);
            m_fd = -1;
            throw std::runtime_error("Failed to preload file");
        }
    }
}

FileDevice::~FileDevice() {
    if (m_fd >= 0) {
        flush();
        close(m_fd);
    }
}

BlockDeviceType FileDevice::type() const {
    return BlockDeviceType::FILE;
}

std::string FileDevice::name() const {
    return m_path;
}

std::string FileDevice::uri() const {
    std::string uri;
    if (m_cached)
        uri += "cached-";
    uri += m_read_only ? "ro" : "rw";
    uri += ":";
    uri += url::encode(m_path);
    if (m_offset > 0 || m_size > 0) {
        uri += ":" + std::to_string(m_offset);
    }
    if (m_size > 0) {
        uri += ":" + std::to_string(m_size);
    }
    return uri;
}

bool FileDevice::isOpen() const {
    return m_fd >= 0;
}

bool FileDevice::read(uint64_t offset, uint8_t* dst, size_t len) {
    if (!dst || len == 0 || offset + len > m_size) {
        return false;
    }

    if (m_cached) {
        memcpy(dst, m_data.data() + offset, len);
        return true;
    }

    ssize_t result = pread(m_fd, dst, len, m_offset + offset);
    return result == static_cast<ssize_t>(len);
}

bool FileDevice::write(uint64_t offset, const uint8_t* src, size_t len) {
    if (!src || len == 0 || offset + len > m_size) {
        return false;
    }

    if (m_read_only) {
        return false;
    }

    if (m_cached) {
        memcpy(m_data.data() + offset, src, len);
        m_dirty = true;
        return true;
    }

    ssize_t result = pwrite(m_fd, src, len, m_offset + offset);
    return result == static_cast<ssize_t>(len);
}

uint64_t FileDevice::size() const {
    return m_size;
}

bool FileDevice::flush() {
    if (m_fd < 0)
        return false;

    if (m_read_only)
        return true;

    if (m_cached && m_dirty) {
        ssize_t result = pwrite(m_fd, m_data.data(), m_size, m_offset);
        if (result == static_cast<ssize_t>(m_size)) {
            m_dirty = false;
            return fsync(m_fd) == 0;
        }
        return false;
    }

    return fsync(m_fd) == 0;
}
