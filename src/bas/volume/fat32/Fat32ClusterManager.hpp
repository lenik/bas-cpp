#ifndef FAT32CLUSTERMANAGER_H
#define FAT32CLUSTERMANAGER_H

#include "../BlockDevice.hpp"

#include <cstdint>
#include <memory>
#include <vector>

class Fat32ClusterManager {
  public:
    Fat32ClusterManager(std::shared_ptr<BlockDevice> device, uint64_t fatOffset, uint32_t fatEntries);

    uint32_t get(uint32_t cluster) const;
    void set(uint32_t cluster, uint32_t value);
    void flushDirty();

    uint32_t findFreeFromHint();
    int64_t findContiguousFreeRun(uint32_t count);
    void setAllocHint(uint32_t cluster);

  private:
    bool isValidCluster(uint32_t cluster) const;
    uint32_t normalizeValue(uint32_t value) const;
    void markDirty(uint32_t cluster);

  private:
    std::shared_ptr<BlockDevice> m_device;
    uint64_t m_fatOffset = 0;
    uint32_t m_fatEntries = 0;
    uint32_t m_allocHint = 2;
    std::vector<uint32_t> m_entries;
    std::vector<uint32_t> m_dirtyClusters;
};

#endif // FAT32CLUSTERMANAGER_H
