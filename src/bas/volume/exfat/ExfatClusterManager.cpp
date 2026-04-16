#include "ExfatClusterManager.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>

namespace {
uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
} // namespace

ExfatClusterManager::ExfatClusterManager(std::shared_ptr<BlockDevice> device, uint64_t fatOffsetBytes,
                                         uint32_t fatEntries, uint64_t bitmapOffsetBytes,
                                         uint32_t bitmapLengthBytes)
    : m_device(std::move(device)),
      m_fatOffsetBytes(fatOffsetBytes),
      m_fatEntries(fatEntries),
      m_bitmapOffsetBytes(bitmapOffsetBytes),
      m_bitmapLengthBytes(bitmapLengthBytes) {
    if (!m_device || m_fatEntries < 2 || m_bitmapLengthBytes == 0) {
        throw IOException("ExfatClusterManager", "", "Invalid manager init");
    }

    std::vector<uint8_t> fatRaw(static_cast<size_t>(m_fatEntries) * 4, 0);
    if (!m_device->read(m_fatOffsetBytes, fatRaw.data(), fatRaw.size())) {
        throw IOException("ExfatClusterManager", m_device->uri(), "Failed loading FAT");
    }
    m_fat.resize(m_fatEntries, 0);
    for (uint32_t i = 0; i < m_fatEntries; ++i) {
        m_fat[i] = readLe32(fatRaw.data() + static_cast<size_t>(i) * 4) & 0x0FFFFFFF;
    }

    m_bitmap.resize(m_bitmapLengthBytes, 0);
    if (!m_device->read(m_bitmapOffsetBytes, m_bitmap.data(), m_bitmap.size())) {
        throw IOException("ExfatClusterManager", m_device->uri(), "Failed loading bitmap");
    }
}

bool ExfatClusterManager::validCluster(uint32_t cluster) const { return cluster >= 2 && cluster < m_fatEntries; }

uint32_t ExfatClusterManager::getFat(uint32_t cluster) const {
    if (!validCluster(cluster)) throw IOException("ExfatClusterManager::getFat", "", "Cluster out of range");
    return m_fat[cluster];
}

void ExfatClusterManager::markFatDirty(uint32_t cluster) { m_dirtyFatClusters.push_back(cluster); }

void ExfatClusterManager::setFat(uint32_t cluster, uint32_t value) {
    if (!validCluster(cluster)) throw IOException("ExfatClusterManager::setFat", "", "Cluster out of range");
    const uint32_t v = value & 0x0FFFFFFF;
    if (m_fat[cluster] == v) return;
    m_fat[cluster] = v;
    markFatDirty(cluster);
}

bool ExfatClusterManager::isAllocated(uint32_t cluster) const {
    if (!validCluster(cluster)) return true;
    const uint32_t idx = cluster - 2;
    const uint32_t b = idx / 8;
    const uint32_t bit = idx % 8;
    if (b >= m_bitmap.size()) return true;
    return (m_bitmap[b] & (1u << bit)) != 0;
}

void ExfatClusterManager::markBitmapDirty(uint32_t byteIndex) { m_dirtyBitmapBytes.push_back(byteIndex); }

void ExfatClusterManager::setAllocated(uint32_t cluster, bool allocated) {
    if (!validCluster(cluster)) throw IOException("ExfatClusterManager::setAllocated", "", "Cluster out of range");
    const uint32_t idx = cluster - 2;
    const uint32_t b = idx / 8;
    const uint32_t bit = idx % 8;
    if (b >= m_bitmap.size()) throw IOException("ExfatClusterManager::setAllocated", "", "Bitmap out of range");
    const uint8_t old = m_bitmap[b];
    if (allocated) m_bitmap[b] |= static_cast<uint8_t>(1u << bit);
    else m_bitmap[b] &= static_cast<uint8_t>(~(1u << bit));
    if (old != m_bitmap[b]) markBitmapDirty(b);
}

void ExfatClusterManager::setAllocHint(uint32_t cluster) {
    if (validCluster(cluster)) m_allocHint = cluster;
}

uint32_t ExfatClusterManager::findFreeFromHint() {
    if (m_fatEntries <= 2) return 0;
    const uint32_t begin = std::max<uint32_t>(2, std::min<uint32_t>(m_allocHint, m_fatEntries - 1));
    for (uint32_t c = begin; c < m_fatEntries; ++c) {
        if (!isAllocated(c)) {
            m_allocHint = (c + 1 < m_fatEntries) ? c + 1 : 2;
            return c;
        }
    }
    for (uint32_t c = 2; c < begin; ++c) {
        if (!isAllocated(c)) {
            m_allocHint = c + 1;
            return c;
        }
    }
    return 0;
}

int64_t ExfatClusterManager::findContiguousFreeRun(uint32_t count) {
    if (count == 0 || m_fatEntries <= 2) return -1;
    const uint32_t begin = std::max<uint32_t>(2, std::min<uint32_t>(m_allocHint, m_fatEntries - 1));
    auto scan = [&](uint32_t s, uint32_t e) -> int64_t {
        uint32_t runStart = 0;
        uint32_t runLen = 0;
        for (uint32_t c = s; c < e; ++c) {
            if (!isAllocated(c)) {
                if (runLen == 0) runStart = c;
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
    int64_t hit = scan(begin, m_fatEntries);
    if (hit >= 0) return hit;
    return scan(2, begin);
}

void ExfatClusterManager::flushDirty() {
    if (!m_dirtyFatClusters.empty()) {
        std::sort(m_dirtyFatClusters.begin(), m_dirtyFatClusters.end());
        m_dirtyFatClusters.erase(std::unique(m_dirtyFatClusters.begin(), m_dirtyFatClusters.end()),
                                 m_dirtyFatClusters.end());
        size_t i = 0;
        while (i < m_dirtyFatClusters.size()) {
            uint32_t start = m_dirtyFatClusters[i];
            uint32_t end = start;
            while (i + 1 < m_dirtyFatClusters.size() && m_dirtyFatClusters[i + 1] == end + 1) {
                ++i;
                ++end;
            }
            const uint32_t count = end - start + 1;
            std::vector<uint8_t> raw(static_cast<size_t>(count) * 4, 0);
            for (uint32_t k = 0; k < count; ++k) {
                const uint32_t v = m_fat[start + k];
                const size_t o = static_cast<size_t>(k) * 4;
                raw[o + 0] = static_cast<uint8_t>(v & 0xFF);
                raw[o + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
                raw[o + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
                raw[o + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
            }
            const uint64_t off = m_fatOffsetBytes + static_cast<uint64_t>(start) * 4;
            if (!m_device->write(off, raw.data(), raw.size())) {
                throw IOException("ExfatClusterManager::flushDirty", m_device->uri(), "Failed flushing FAT");
            }
            ++i;
        }
        m_dirtyFatClusters.clear();
    }

    if (!m_dirtyBitmapBytes.empty()) {
        std::sort(m_dirtyBitmapBytes.begin(), m_dirtyBitmapBytes.end());
        m_dirtyBitmapBytes.erase(std::unique(m_dirtyBitmapBytes.begin(), m_dirtyBitmapBytes.end()),
                                 m_dirtyBitmapBytes.end());
        size_t i = 0;
        while (i < m_dirtyBitmapBytes.size()) {
            uint32_t start = m_dirtyBitmapBytes[i];
            uint32_t end = start;
            while (i + 1 < m_dirtyBitmapBytes.size() && m_dirtyBitmapBytes[i + 1] == end + 1) {
                ++i;
                ++end;
            }
            const uint32_t count = end - start + 1;
            const uint64_t off = m_bitmapOffsetBytes + start;
            if (!m_device->write(off, m_bitmap.data() + start, count)) {
                throw IOException("ExfatClusterManager::flushDirty", m_device->uri(),
                                  "Failed flushing bitmap");
            }
            ++i;
        }
        m_dirtyBitmapBytes.clear();
    }
}
