#ifndef FILEDEVICE_H
#define FILEDEVICE_H

#include "../BlockDevice.hpp"

#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

class FileDevice : public BlockDevice {
  public:
    FileDevice(const std::string& path, uint64_t offset, uint64_t length, bool read_only = false,
               bool cached = false);
    ~FileDevice() override;

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
    bool m_read_only;
    bool m_cached;
    int m_fd;
    std::vector<uint8_t> m_data;
    bool m_dirty = false;
};

#endif // FILEDEVICE_H
