#include "Fat32ClusterManager.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>

namespace {
uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
} // namespace

Fat32ClusterManager::Fat32ClusterManager(std::shared_ptr<BlockDevice> device, uint64_t fatOffset,
                                         uint32_t fatEntries)
    : m_device(std::move(device)), m_fatOffset(fatOffset), m_fatEntries(fatEntries) {
    if (!m_device || m_fatEntries < 2) {
        throw IOException("Fat32ClusterManager", "", "Invalid FAT manager init");
    }
    m_entries.resize(m_fatEntries, 0);
    std::vector<uint8_t> fatRaw(static_cast<size_t>(m_fatEntries) * 4, 0);
    if (!m_device->read(m_fatOffset, fatRaw.data(), fatRaw.size())) {
        throw IOException("Fat32ClusterManager", m_device->uri(), "Failed to load FAT table");
    }
    for (uint32_t i = 0; i < m_fatEntries; ++i) {
        m_entries[i] = readLe32(fatRaw.data() + static_cast<size_t>(i) * 4) & 0x0FFFFFFF;
    }
}

bool Fat32ClusterManager::isValidCluster(uint32_t cluster) const { return cluster < m_fatEntries; }

uint32_t Fat32ClusterManager::normalizeValue(uint32_t value) const { return value & 0x0FFFFFFF; }

uint32_t Fat32ClusterManager::get(uint32_t cluster) const {
    if (!isValidCluster(cluster)) {
        throw IOException("Fat32ClusterManager::get", "", "FAT cluster out of range");
    }
    return m_entries[cluster];
}

void Fat32ClusterManager::markDirty(uint32_t cluster) { m_dirtyClusters.push_back(cluster); }

void Fat32ClusterManager::set(uint32_t cluster, uint32_t value) {
    if (!isValidCluster(cluster)) {
        throw IOException("Fat32ClusterManager::set", "", "FAT cluster out of range");
    }
    const uint32_t v = normalizeValue(value);
    if (m_entries[cluster] == v) {
        return;
    }
    m_entries[cluster] = v;
    markDirty(cluster);
}

void Fat32ClusterManager::flushDirty() {
    if (m_dirtyClusters.empty()) {
        return;
    }
    std::sort(m_dirtyClusters.begin(), m_dirtyClusters.end());
    m_dirtyClusters.erase(std::unique(m_dirtyClusters.begin(), m_dirtyClusters.end()),
                          m_dirtyClusters.end());

    size_t i = 0;
    while (i < m_dirtyClusters.size()) {
        uint32_t start = m_dirtyClusters[i];
        uint32_t end = start;
        while (i + 1 < m_dirtyClusters.size() && m_dirtyClusters[i + 1] == end + 1) {
            ++i;
            ++end;
        }

        const uint32_t count = end - start + 1;
        std::vector<uint8_t> block(static_cast<size_t>(count) * 4, 0);
        for (uint32_t k = 0; k < count; ++k) {
            const uint32_t v = m_entries[start + k];
            const size_t o = static_cast<size_t>(k) * 4;
            block[o + 0] = static_cast<uint8_t>(v & 0xFF);
            block[o + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            block[o + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            block[o + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        }
        const uint64_t off = m_fatOffset + static_cast<uint64_t>(start) * 4;
        if (!m_device->write(off, block.data(), block.size())) {
            throw IOException("Fat32ClusterManager::flushDirty", m_device->uri(),
                              "Failed to flush FAT dirty region");
        }
        ++i;
    }
    m_dirtyClusters.clear();
}

void Fat32ClusterManager::setAllocHint(uint32_t cluster) {
    if (cluster >= 2 && cluster < m_fatEntries) {
        m_allocHint = cluster;
    }
}

uint32_t Fat32ClusterManager::findFreeFromHint() {
    if (m_fatEntries <= 2) {
        return 0;
    }
    const uint32_t begin = std::max<uint32_t>(2, std::min<uint32_t>(m_allocHint, m_fatEntries - 1));
    for (uint32_t c = begin; c < m_fatEntries; ++c) {
        if (m_entries[c] == 0) {
            m_allocHint = (c + 1 < m_fatEntries) ? c + 1 : 2;
            return c;
        }
    }
    for (uint32_t c = 2; c < begin; ++c) {
        if (m_entries[c] == 0) {
            m_allocHint = c + 1;
            return c;
        }
    }
    return 0;
}

int64_t Fat32ClusterManager::findContiguousFreeRun(uint32_t count) {
    if (count == 0 || m_fatEntries <= 2) {
        return -1;
    }
    const uint32_t begin = std::max<uint32_t>(2, std::min<uint32_t>(m_allocHint, m_fatEntries - 1));
    auto search = [&](uint32_t s, uint32_t e) -> int64_t {
        uint32_t runStart = 0;
        uint32_t runLen = 0;
        for (uint32_t c = s; c < e; ++c) {
            if (m_entries[c] == 0) {
                if (runLen == 0) {
                    runStart = c;
                }
                if (++runLen >= count) {
                    m_allocHint = runStart + count;
                    return static_cast<int64_t>(runStart);
                }
            } else {
                runLen = 0;
            }
        }
        return -1;
    };
    int64_t hit = search(begin, m_fatEntries);
    if (hit >= 0) {
        return hit;
    }
    return search(2, begin);
}
