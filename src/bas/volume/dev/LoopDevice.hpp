#ifndef LOOPDEVICE_H
#define LOOPDEVICE_H

#include "../BlockDevice.hpp"

#include <memory>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <string>

class LoopDevice : public BlockDevice {
public:
    LoopDevice(std::shared_ptr<BlockDevice> device, uint64_t offset, uint64_t length);

    BlockDeviceType type() const override;
    std::string name() const override;
    std::string uri() const override;
    bool isOpen() const override;
    bool read(uint64_t offset, uint8_t* dst, size_t len) override;
    bool write(uint64_t offset, const uint8_t* src, size_t len) override;
    uint64_t size() const override;
    bool flush() override;

private:
    std::shared_ptr<BlockDevice> m_device;
    uint64_t m_offset;
    uint64_t m_size;
};

#endif // LOOPDEVICE_H