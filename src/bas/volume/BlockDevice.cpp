#include "BlockDevice.hpp"

#include "dev/FileDevice.hpp"
#include "dev/LoopDevice.hpp"
#include "dev/MMapDevice.hpp"
#include "dev/MemDevice.hpp"

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Static member initialization
std::atomic<uint64_t> BlockDevice::s_next_id{1};

std::shared_ptr<BlockDevice> BlockDevice::file(const std::string& path, uint64_t offset,
                                               uint64_t length, bool read_only, bool cached) {
    return std::make_shared<FileDevice>(path, offset, length, read_only, cached);
}

std::shared_ptr<BlockDevice> BlockDevice::load(const std::string& path, uint64_t offset,
                                               uint64_t length, bool read_only) {
    return std::make_shared<FileDevice>(path, offset, length, read_only, true);
}

std::shared_ptr<BlockDevice> BlockDevice::mmap(const std::string& path, uint64_t offset,
                                               uint64_t length) {
    return std::make_shared<MMapDevice>(path, offset, length);
}

std::shared_ptr<BlockDevice> BlockDevice::mem(const void* data, size_t size) {
    return std::make_shared<MemDevice>(data, size, true);
}

std::shared_ptr<BlockDevice> BlockDevice::loop(std::shared_ptr<BlockDevice> device, uint64_t offset,
                                               uint64_t length) {
    return std::make_shared<LoopDevice>(device, offset, length);
}
