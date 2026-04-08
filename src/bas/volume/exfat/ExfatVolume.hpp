#ifndef EXFATVOLUME_H
#define EXFATVOLUME_H

#include "../BlockDevice.hpp"
#include "../MountOptions.hpp"
#include "../Volume.hpp"

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * exFAT filesystem mount options.
 */
struct ExfatOptions : public MountOptions {};

class ExfatVolume : public Volume {
  public:
    struct Dirent {
        bool isDirectory = false;
        uint32_t firstCluster = 0;
        uint64_t size = 0;
        time_t mtime = 0;
        time_t ctime = 0;
        time_t atime = 0;
        bool childrenParsed = false;
        std::unordered_map<std::string, Dirent*> children;

        void copyTo(DirNode& node) const;
    };

  private:
    std::shared_ptr<BlockDevice> m_device;
    ExfatOptions m_options;

    // Boot sector info
    uint16_t m_bytesPerSector = 0;
    uint8_t m_sectorsPerCluster = 0;
    uint32_t m_clusterSize = 0;
    uint32_t m_fatOffset = 0;
    uint32_t m_fatLength = 0;
    uint32_t m_clusterHeapOffset = 0;
    uint32_t m_clusterCount = 0;
    uint32_t m_rootCluster = 0;
    uint32_t m_volumeSerial = 0;

    // Allocation bitmap
    uint32_t m_bitmapCluster = 0;
    uint32_t m_bitmapLength = 0;

    // Upcase table
    uint32_t m_upcaseCluster = 0;
    uint32_t m_upcaseLength = 0;
    std::vector<uint16_t> m_upcaseTable;

    mutable std::unordered_map<std::string, Dirent> m_dirents;
    mutable std::unordered_map<uint32_t, std::shared_ptr<Dirent>> m_rtnodes;

    // Cluster chain cache
    mutable std::unordered_map<uint32_t, std::vector<uint32_t>> m_chainCache;

  public:
    // File-backed volume
    explicit ExfatVolume(std::shared_ptr<BlockDevice> device, const ExfatOptions& options);

    std::string getClass() const override { return "exfat"; }
    std::string getUrl() const override { return "exfat:" + m_device->uri(); }
    std::string getDeviceUrl() const override { return m_device->uri(); }
    VolumeType getType() const override { return VolumeType::ARCHIVE; }
    bool isLocal() const override { return false; }

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, DirNode* status) const override;

    void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path,
                                                  bool append = false) override;
    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.",
                               std::string_view suffix = "") override;

  protected:
    std::string getDefaultLabel() const override;

    std::vector<uint8_t> readFileUnchecked(std::string_view path, int64_t off = 0,
                                           size_t len = 0) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;

    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) override;

  private:
    void parseBootSector();
    void parseBitmap();
    void parseUpcaseTable();
    void buildIndex();

    uint64_t clusterToOffset(uint32_t cluster) const;
    uint32_t getFatEntry(uint32_t cluster) const;
    void setFatEntry(uint32_t cluster, uint32_t value);
    std::vector<uint32_t> readClusterChain(uint32_t firstCluster) const;
    bool readAt(uint64_t offset, uint8_t* dst, size_t len) const;
    bool writeAt(uint64_t offset, const uint8_t* src, size_t len);

    std::string normalizeArg(std::string_view path) const;
    std::string getParentPath(std::string_view path) const;
    std::string getFileName(std::string_view path) const;

    bool ensurePathIndexed(std::string_view path) const;
    bool ensureDirectoryParsed(std::string_view dirPath) const;
    void parseDirectoryCluster(std::string_view dirPath, uint32_t firstCluster) const;

    uint32_t allocateCluster(uint32_t prevCluster = 0);
    void freeClusterChain(uint32_t firstCluster);
    void writeClusterChain(uint32_t firstCluster, const std::vector<uint8_t>& data);
    uint32_t findFreeCluster() const;

    // Directory entry management
    void createFileEntry(std::string_view path, uint32_t firstCluster, uint64_t size);
    void updateFileEntry(std::string_view path, uint32_t firstCluster, uint64_t size);
    void deleteFileEntry(std::string_view path);
    void createDirectoryEntry(std::string_view path, uint32_t firstCluster);
    void deleteDirectoryEntry(std::string_view path);

    // exFAT specific
    uint16_t upcaseChar(uint16_t ch) const;
    uint32_t computeChecksum(const uint8_t* data, size_t len, uint32_t initial = 0) const;
    void writeDirectoryEntries(uint32_t dirCluster,
                               const std::vector<std::vector<uint8_t>>& entries);

    void invalidateCache(std::string_view path);
};

#endif // EXFATVOLUME_H
