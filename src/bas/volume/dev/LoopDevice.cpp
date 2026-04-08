#include "LoopDevice.hpp"

#include "../../util/urls.hpp"

#include <stdexcept>

LoopDevice::LoopDevice(std::shared_ptr<BlockDevice> device, uint64_t offset, uint64_t length)
    : m_device(device), m_offset(offset), m_size(0) {
    if (!device || !device->isOpen()) {
        throw std::runtime_error("Invalid underlying device");
    }

    uint64_t deviceSize = device->size();

    if (offset >= deviceSize) {
        throw std::runtime_error("Offset beyond device size");
    }

    if (length > 0 && length < deviceSize - offset) {
        m_size = length;
    } else {
        m_size = deviceSize - offset;
    }
}

BlockDeviceType LoopDevice::type() const {
    return BlockDeviceType::LOOP;
}

std::string LoopDevice::name() const {
    return "loop:" + m_device->name();
}

std::string LoopDevice::uri() const {
    std::string underlyingUri = m_device->uri();
    std::string encoded = url::encode(underlyingUri);
    std::string uri = "loop:" + encoded;
    if (m_offset > 0 || m_size > 0) {
        uri += ":" + std::to_string(m_offset);
    }
    if (m_size > 0) {
        uri += ":" + std::to_string(m_size);
    }
    return uri;
}

bool LoopDevice::isOpen() const {
    return m_device && m_device->isOpen();
}

bool LoopDevice::read(uint64_t offset, uint8_t* dst, size_t len) {
    if (!m_device || !dst || len == 0 || offset + len > m_size) {
        return false;
    }
    return m_device->read(m_offset + offset, dst, len);
}

bool LoopDevice::write(uint64_t offset, const uint8_t* src, size_t len) {
    if (!m_device || !src || len == 0 || offset + len > m_size) {
        return false;
    }
    return m_device->write(m_offset + offset, src, len);
}

uint64_t LoopDevice::size() const {
    return m_size;
}

bool LoopDevice::flush() {
    return m_device ? m_device->flush() : false;
}
