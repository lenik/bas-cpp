#ifndef EXFATCLUSTERMANAGER_H
#define EXFATCLUSTERMANAGER_H

#include "../BlockDevice.hpp"

#include <cstdint>
#include <memory>
#include <vector>

class ExfatClusterManager {
  public:
    ExfatClusterManager(std::shared_ptr<BlockDevice> device, uint64_t fatOffsetBytes,
                        uint32_t fatEntries, uint64_t bitmapOffsetBytes, uint32_t bitmapLengthBytes);

    uint32_t getFat(uint32_t cluster) const;
    void setFat(uint32_t cluster, uint32_t value);

    bool isAllocated(uint32_t cluster) const;
    void setAllocated(uint32_t cluster, bool allocated);

    uint32_t findFreeFromHint();
    int64_t findContiguousFreeRun(uint32_t count);
    void setAllocHint(uint32_t cluster);

    void flushDirty();

  private:
    bool validCluster(uint32_t cluster) const;
    void markFatDirty(uint32_t cluster);
    void markBitmapDirty(uint32_t byteIndex);

  private:
    std::shared_ptr<BlockDevice> m_device;
    uint64_t m_fatOffsetBytes = 0;
    uint32_t m_fatEntries = 0;
    uint64_t m_bitmapOffsetBytes = 0;
    uint32_t m_bitmapLengthBytes = 0;
    uint32_t m_allocHint = 2;

    std::vector<uint32_t> m_fat;
    std::vector<uint8_t> m_bitmap;
    std::vector<uint32_t> m_dirtyFatClusters;
    std::vector<uint32_t> m_dirtyBitmapBytes;
};

#endif // EXFATCLUSTERMANAGER_H
