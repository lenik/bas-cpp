#include "BlockDevice.hpp"

#include "dev/FileDevice.hpp"
#include "dev/LoopDevice.hpp"
#include "dev/MMapDevice.hpp"
#include "dev/MemDevice.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <sys/mman.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

// Static member initialization
std::atomic<uint64_t> BlockDevice::s_next_id{1};

std::string BlockDevice::qName() const {
    return typeString() + ":" + name();
}

const FsInfo& BlockDevice::fsInfo() {
    loadInfo();
    return m_info;
}

std::string BlockDevice::uuid() {
    loadInfo();
    return m_info.uuid;
}

std::string BlockDevice::label() {
    loadInfo();
    return m_info.label;
}

long BlockDevice::capacity() {
    loadInfo();
    return static_cast<long>(m_info.size);
}

long BlockDevice::available() {
    loadInfo();
    return static_cast<long>(m_info.free);
}

void BlockDevice::loadInfo() {
    if (m_info_valid)
        return;

    FsInfo info{};
    info.size = size();

    const uint64_t devSize = size();
    if (devSize > 0 && isOpen()) {
        const uint64_t want = std::min<uint64_t>(devSize, kFsMagicProbeSize);
        std::vector<uint8_t> buf(static_cast<size_t>(want));
        if (read(0, buf.data(), buf.size())) {
            info = probeFsMagic(buf.data(), buf.size());
            if (!info.detected) {
                info = FsInfo{};
                info.size = devSize;
            } else if (info.size == 0) {
                info.size = devSize;
            }
        }
    }

    // Path-backed fallback when magic did not yield a UUID.
    if (info.uuid.empty() && type() == BlockDeviceType::FILE) {
        const std::string path = name();
        if (!path.empty() && path.front() == '/') {
            std::string fromBlkid = readUuid(path);
            if (!fromBlkid.empty())
                info.uuid = std::move(fromBlkid);
        }
    }

    m_info = std::move(info);
    m_info_valid = true;
}

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
    return std::make_shared<MemDevice>(const_cast<void*>(data), size);
}

std::shared_ptr<BlockDevice> BlockDevice::loop(std::shared_ptr<BlockDevice> device, uint64_t offset,
                                               uint64_t length) {
    return std::make_shared<LoopDevice>(device, offset, length);
}

std::string BlockDevice::readUuid(const std::string& device) {
    if (device.empty()) {
        return "";
    }

    std::string command = "blkid -s UUID -o value " + device + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string uuid(buffer);
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\n'), uuid.end());
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\r'), uuid.end());
            pclose(pipe);
            if (!uuid.empty()) {
                return uuid;
            }
        }
        pclose(pipe);
    }

    try {
        for (const auto& entry : fs::directory_iterator("/dev/disk/by-uuid")) {
            if (fs::is_symlink(entry.path())) {
                try {
                    fs::path symlinkPath = entry.path();
                    fs::path canonicalTarget = fs::canonical(symlinkPath);
                    fs::path canonicalDevice = fs::canonical(device);

                    if (canonicalTarget == canonicalDevice) {
                        return entry.path().filename().string();
                    }
                } catch (...) {
                    continue;
                }
            }
        }
    } catch (...) {
    }

    return "";
}
