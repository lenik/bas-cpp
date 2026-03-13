#include "VolumeManager.hpp"

#include "LocalVolume.hpp"
#include "Volume.hpp"

#include <log/uselog.h>

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

VolumeManager::VolumeManager() {
}

VolumeManager::~VolumeManager() {
}

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

void VolumeManager::clear() {
    m_volumes.clear();
}

size_t VolumeManager::getVolumeCount() const {
    return m_volumes.size();
}

Volume* VolumeManager::getVolume(size_t index) const {
    return m_volumes[index].get();
}

Volume* VolumeManager::getDefaultVolume() const {
    if (m_volumes.empty()) {
        return nullptr;
    }
    return m_volumes[0].get();
}

const std::vector<std::unique_ptr<Volume>>& VolumeManager::all() const {
    return m_volumes;
}

std::vector<Volume*> VolumeManager::type(std::string_view type) const {
    std::vector<Volume*> matches;
    for (const auto& volume : m_volumes) {
        if (volume && volume->getClass() == type) {
            matches.push_back(volume.get());
        }
    }
    return matches;
}

void VolumeManager::addLocalVolumes() {
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

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.size() < 5) {
            continue;
        }

        // Find "-" separator to get filesystem type and device
        size_t dashIdx = 0;
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "-") {
                dashIdx = i;
                break;
            }
        }
        if (dashIdx == 0 || dashIdx + 2 >= tokens.size()) {
            continue;
        }

        std::string mountPoint = tokens[4];
        std::string fsType = tokens[dashIdx + 1];

        // Skip pseudo filesystems and ones we don't want to expose as volumes
        if (mountPoint.find("/dev/") == 0
            || mountPoint.find("/proc/") == 0
            || mountPoint.find("/run/") == 0
            || mountPoint.find("/sys/") == 0
            || mountPoint.find("/tmp/") == 0
            ) {
            continue;
        }
        if (fsType == "cgroup" 
            || fsType == "cgroup2" 
            || fsType == "devpts" 
            || fsType == "devtmpfs" 
            || fsType == "overlay"
            || fsType == "proc" 
            || fsType == "sysfs" 
            || fsType == "tmpfs" 
        ) {
            continue;
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
