#ifndef MEMDEVICE_H
#define MEMDEVICE_H

#include "../BlockDevice.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

enum OwnType {
    NONE,
    AUTO,
    ARRAY,
    MALLOC,
};

class MemDevice : public BlockDevice {
  public:
    MemDevice(void* data, size_t size, std::string_view name = "", OwnType ownType = OwnType::AUTO);
    ~MemDevice() override;

    BlockDeviceType type() const override;
    std::string name() const override;
    std::string uri() const override;
    uint64_t size() const override;
    bool read(uint64_t offset, uint8_t* dst, size_t len) override;
    bool write(uint64_t offset, const uint8_t* src, size_t len) override;
    bool isOpen() const override;

  private:
    std::string m_name;
    OwnType m_ownType = OwnType::ARRAY;
    uint8_t* m_data = nullptr;
    size_t m_size = 0;
};

#endif // MEMDEVICE_H