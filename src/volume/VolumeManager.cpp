#include "VolumeManager.hpp"

#include "LocalVolume.hpp"
#include "Volume.hpp"
#include "mountinfo.hpp"

#include "ext4/Ext4Volume.hpp"
#include "fat32/Fat32Volume.hpp"

#include <cctype>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <log/uselog.h>

VolumeManager::VolumeManager() {}

VolumeManager::~VolumeManager() {}

bool VolumeManager::addVolume(std::unique_ptr<Volume> volume) {
    for (const auto& transformer : m_transformers) {
        volume = transformer(std::move(volume));
        if (!volume) {
            return false;
        }
    }
    m_volumes.push_back(std::move(volume));
    return true;
}

void VolumeManager::removeVolume(size_t index) {
    if (index >= m_volumes.size())
        throw std::out_of_range("Index out of range");
    m_volumes.erase(m_volumes.begin() + index);
}

void VolumeManager::clear() { m_volumes.clear(); }

size_t VolumeManager::getVolumeCount() const { return m_volumes.size(); }

Volume* VolumeManager::getVolume(size_t index) const { return m_volumes[index].get(); }

Volume* VolumeManager::getDefaultVolume() const {
    if (m_volumes.empty()) {
        return nullptr;
    }
    return m_volumes[0].get();
}

const std::vector<std::unique_ptr<Volume>>& VolumeManager::all() const { return m_volumes; }

std::vector<Volume*> VolumeManager::type(std::string_view type) const {
    std::vector<Volume*> matches;
    for (const auto& volume : m_volumes) {
        if (volume && volume->getClass() == type) {
            matches.push_back(volume.get());
        }
    }
    return matches;
}

std::optional<VolumeFile> VolumeManager::resolveUri(std::string_view uri) const {
    constexpr std::string_view prefix = "vol://";
    if (uri.size() <= prefix.size()) {
        return std::nullopt;
    }
    if (uri.substr(0, prefix.size()) != prefix) {
        return std::nullopt;
    }

    std::string_view rest = uri.substr(prefix.size());
    if (rest.empty()) {
        return std::nullopt;
    }

    // <index><path> where <path> always starts with '/'
    const size_t firstSlash = rest.find('/');
    if (firstSlash == std::string_view::npos) {
        return std::nullopt;
    }

    std::string_view indexPart = rest.substr(0, firstSlash);
    std::string_view pathPart = rest.substr(firstSlash);

    if (indexPart.empty() || pathPart.empty() || pathPart.front() != '/') {
        return std::nullopt;
    }

    for (char c : indexPart) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return std::nullopt;
        }
    }

    size_t index = 0;
    try {
        index = static_cast<size_t>(std::stoull(std::string(indexPart)));
    } catch (...) {
        return std::nullopt;
    }

    if (index >= m_volumes.size()) {
        return std::nullopt;
    }

    Volume* volume = m_volumes[index].get();
    if (!volume) {
        return std::nullopt;
    }

    std::string normalizedPath;
    try {
        normalizedPath = volume->normalize(pathPart);
    } catch (...) {
        return std::nullopt;
    }

    return VolumeFile(volume, normalizedPath);
}

bool VolumeManager::addFat32Volume(std::string_view imagePath) {
    if (imagePath.empty()) {
        throw std::invalid_argument("VolumeManager::addFat32Volume: imagePath is required");
    }
    return addVolume(std::make_unique<Fat32Volume>(imagePath));
}

bool VolumeManager::addExt4Volume(std::string_view imagePath) {
    if (imagePath.empty()) {
        throw std::invalid_argument("VolumeManager::addExt4Volume: imagePath is required");
    }
    return addVolume(std::make_unique<Ext4Volume>(imagePath));
}

void VolumeManager::addLocalVolumes(bool includeSymbols, bool excludeReadOnly) {
#if defined(__linux__)
    std::ifstream mountinfo("/proc/self/mountinfo");
    if (!mountinfo.is_open()) {
        return;
    }

    std::unordered_set<std::string> seenMounts;
    std::string line;
    while (std::getline(mountinfo, line)) {
        if (line.empty()) {
            continue;
        }

        MountInfo mountInfo;
        if (!parseMountInfoLine(line, mountInfo)) {
            continue;
        }

        std::string root = mountInfo.root;
        std::string mountPoint = mountInfo.mountPoint;
        std::string fsType = mountInfo.fsType;
        std::string vfsOpts = mountInfo.vfsOpts;
        std::string superOpts = mountInfo.superOpts;

        // Skip pseudo filesystems and ones we don't want to expose as volumes
        if (mountPoint.find("/dev/") == 0     //
            || mountPoint.find("/proc/") == 0 //
            || mountPoint.find("/run/") == 0  //
            || mountPoint.find("/sys/") == 0  //
            || mountPoint.find("/tmp/") == 0) {
            continue;
        }

        if (mountInfo.device.find("/") == std::string::npos)
            continue;

        if (fsType == "cgroup"      //
            || fsType == "cgroup2"  //
            || fsType == "devpts"   //
            || fsType == "devtmpfs" //
            || fsType == "proc"     //
            || fsType == "sysfs"    //
            || fsType == "tmpfs") {
            continue;
        }

        if (!includeSymbols) {
            if (fsType == "overlay")
                continue;
            if (vfsOpts.find("bind") != std::string::npos)
                continue;
            if (root != "/")
                continue;
        }

        if (excludeReadOnly) {
            if (vfsOpts.find("ro") != std::string::npos //
                || superOpts.find("ro") != std::string::npos) {
                continue;
            }
        }

        if (seenMounts.find(mountPoint) != seenMounts.end()) {
            continue;
        }
        seenMounts.insert(mountPoint);

        try {
            addVolume(std::make_unique<LocalVolume>(mountPoint));
        } catch (const std::exception& e) {
            logwarn_fmt("addLocalVolumes: failed for mount {}: {}", mountPoint.c_str(), e.what());
        }
    }
#else
    // Fallback: at least expose root filesystem
    try {
        addVolume(std::make_unique<LocalVolume>("/"));
    } catch (...) {
    }
#endif
}

void VolumeManager::addTransformer(VolumeTransformer transformer) {
    m_transformers.push_back(transformer);
}
