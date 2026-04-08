#include "MemDevice.hpp"

#include <stdexcept>

MemDevice::MemDevice(void* data, size_t size, std::string_view name, OwnType ownType)
    : m_name(name), m_size(size) {
    if (ownType == OwnType::AUTO) {
        if (data) {
            ownType = OwnType::NONE;
        } else {
            ownType = OwnType::MALLOC;
            data = malloc(size);
            if (data == nullptr) {
                throw std::runtime_error("Failed to allocate memory");
            }
        }
    }
    if (data == nullptr) {
        throw std::runtime_error("data pointer is null");
    }
    m_data = static_cast<uint8_t*>(data);
    m_ownType = ownType;
}

MemDevice::~MemDevice() {
    if (m_data) {
        if (m_ownType == OwnType::MALLOC) {
            free(m_data);
        } else if (m_ownType == OwnType::ARRAY) {
            delete[] m_data;
        }
    }
}

BlockDeviceType MemDevice::type() const { return BlockDeviceType::MEM; }

std::string MemDevice::name() const {
    std::string uri = "mem:";
    if (m_name.empty())
        uri += std::to_string(reinterpret_cast<uintptr_t>(m_data));
    else
        uri += m_name;
    uri += ":" + std::to_string(m_size);
    return uri;
}

std::string MemDevice::uri() const {
    std::string uri = "mem:" + std::to_string(reinterpret_cast<uintptr_t>(m_data));
    uri += ":" + std::to_string(m_size);
    return uri;
}

uint64_t MemDevice::size() const { return m_size; }

bool MemDevice::isOpen() const { return m_data != nullptr; }

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
