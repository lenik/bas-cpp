#ifndef MMAPDEVICE_H
#define MMAPDEVICE_H

#include "../BlockDevice.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstring>

class MMapDevice : public BlockDevice {
public:
    MMapDevice(const std::string& path, uint64_t offset, uint64_t length);
    ~MMapDevice() override;

    BlockDeviceType type() const override;
    std::string name() const override;
    std::string uri() const override;
    bool isOpen() const override;
    bool read(uint64_t offset, uint8_t* dst, size_t len) override;
    bool write(uint64_t offset, const uint8_t* src, size_t len) override;
    uint64_t size() const override;
    bool flush() override;

private:
    std::string m_path;
    uint64_t m_offset;
    uint64_t m_size;

    int m_fd;
    void* m_data;
};

#endif // MMAPDEVICE_H