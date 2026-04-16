#include "MMapDevice.hpp"

#include <stdexcept>

MMapDevice::MMapDevice(const std::string& path, uint64_t offset, uint64_t length)
    : m_path(path), m_offset(offset), m_size(0), m_fd(-1), m_data(nullptr) {
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
    if (offset >= m_size) {
        close(m_fd);
        m_fd = -1;
        throw std::runtime_error("Offset beyond file size");
    }

    if (length > 0 && length < m_size - offset) {
        m_size = length;
    }

    // Memory map
    m_data = ::mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, offset);
    if (m_data == MAP_FAILED) {
        close(m_fd);
        m_fd = -1;
        m_data = nullptr;
        throw std::runtime_error("Failed to mmap file");
    }
}

MMapDevice::~MMapDevice() {
    if (m_data && m_data != MAP_FAILED) {
        flush();
        munmap(m_data, m_size);
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
}

BlockDeviceType MMapDevice::type() const {
    return BlockDeviceType::MMAP;
}

std::string MMapDevice::name() const {
    return "mmap:" + m_path;
}

std::string MMapDevice::uri() const {
    std::string uri = "mmap:" + m_path;
    if (m_offset > 0 || m_size > 0) {
        uri += ":" + std::to_string(m_offset);
    }
    if (m_size > 0) {
        uri += ":" + std::to_string(m_size);
    }
    return uri;
}

bool MMapDevice::isOpen() const {
    return m_fd >= 0 && m_data != nullptr;
}

bool MMapDevice::read(uint64_t offset, uint8_t* dst, size_t len) {
    if (!m_data || !dst || len == 0 || offset + len > m_size) {
        return false;
    }
    memcpy(dst, static_cast<uint8_t*>(m_data) + offset, len);
    return true;
}

bool MMapDevice::write(uint64_t offset, const uint8_t* src, size_t len) {
    if (!m_data || !src || len == 0 || offset + len > m_size) {
        return false;
    }
    memcpy(static_cast<uint8_t*>(m_data) + offset, src, len);
    return true;
}

uint64_t MMapDevice::size() const {
    return m_size;
}

bool MMapDevice::flush() {
    if (!m_data || m_data == MAP_FAILED)
        return false;
    return ::msync(m_data, m_size, MS_SYNC) == 0;
}
