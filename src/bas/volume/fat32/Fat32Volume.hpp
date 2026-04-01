#ifndef FAT32VOLUME_H
#define FAT32VOLUME_H

#include "../Volume.hpp"
#include "../MountOptions.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class Fat32Volume : public Volume {
  public:

    struct Dirent;
    using DirentObj = std::shared_ptr<Dirent>;
    using DirentRef = std::weak_ptr<Dirent>;

    struct Dirent {
        bool isDirectory = false;
        uint32_t firstCluster = 0;
        uint32_t size = 0;
        time_t atime = 0;
        time_t mtime = 0;
        time_t creationTime = 0;
        bool childrenParsed = false;
        std::unordered_map<std::string, Dirent*> children;

        void copyTo(DirNode& node) const;
    };

private:
    std::string m_imagePath;
    
    // Memory-backed support
    const uint8_t* m_memoryRegion = nullptr;
    size_t m_memorySize = 0;
    uint64_t m_imageSize = 0;
    bool m_ownMemory = false;  // If true, we own the memory and will free it
    std::unique_ptr<uint8_t[]> m_managedMemory;  // Owned memory buffer
    
    MountOptions m_mountOptions;

    uint16_t m_bytesPerSector = 0;
    uint8_t m_sectorsPerCluster = 0;
    uint16_t m_reservedSectors = 0;
    uint8_t m_numFATs = 0;
    uint32_t m_sectorsPerFAT = 0;
    uint32_t m_rootCluster = 0;

    uint64_t m_fatOffset = 0;
    uint64_t m_dataOffset = 0;
    uint32_t m_clusterSize = 0;

    mutable std::unordered_map<std::string, Dirent> m_dirents;

  public:
    // File-backed volume
    explicit Fat32Volume(std::string_view imagePath);
    
    // Memory-backed volume
    explicit Fat32Volume(const uint8_t* memoryRegion, size_t size);
    
    // Volume with mount options
    explicit Fat32Volume(const MountOptions& options);

    std::string getClass() const override { return "fat32"; }
    std::string getId() const override { return m_mountOptions.isMemoryBacked() ? "memory" : m_imagePath; }
    VolumeType getType() const override { return VolumeType::ARCHIVE; }
    std::string getSource() const override;
    bool isLocal() const override { return false; }
    std::string getLocalFile(std::string_view path) const override;

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, DirNode* status) const override;
    void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path,
                                                  bool append = false) override;
    std::unique_ptr<Reader> newReader(std::string_view path,
                                      std::string_view encoding = "UTF-8") override;
    std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false,
                                      std::string_view encoding = "UTF-8") override;

    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;
    std::unique_ptr<RandomReader> newRandomReader(std::string_view path,
                                                  std::string_view encoding = "UTF-8") override;

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
    void buildIndex();
    void parseDirectoryCluster(std::string_view dirPath, uint32_t firstCluster) const;
    bool ensureDirectoryParsed(std::string_view dirPath) const;
    bool ensurePathIndexed(std::string_view path) const;

    std::vector<uint32_t> readClusterChain(uint32_t firstCluster) const;
    uint32_t getFatEntry(uint32_t cluster) const;
    void setFatEntry(uint32_t cluster, uint32_t value);
    uint64_t clusterToOffset(uint32_t cluster) const;
    bool readAt(uint64_t offset, uint8_t* dst, size_t len) const;
    bool writeAt(uint64_t offset, const uint8_t* src, size_t len);

    std::string normalizePath(std::string_view path) const;
    std::string decodeShortName(const uint8_t* entry) const;
    std::string decodeLfnChunk(const uint8_t* entry) const;

    // Write support
    uint32_t allocateCluster(uint32_t prevCluster);
    void freeClusterChain(uint32_t firstCluster);
    void writeClusterChain(uint32_t firstCluster, const std::vector<uint8_t>& data);
    uint32_t findFreeCluster() const;
    
    void createFileEntry(std::string_view path, uint32_t firstCluster, uint32_t size);
    void updateFileEntry(std::string_view path, uint32_t firstCluster, uint32_t size);
    void deleteFileEntry(std::string_view path);
    void createDirectoryEntry(std::string_view path, uint32_t firstCluster);
    void deleteDirectoryEntry(std::string_view path);
    
    std::string getParentPath(std::string_view path) const;
    std::string getFileName(std::string_view path) const;
    
    void invalidateIndex();
    
    // Directory entry writing to disk
    void writeDirectoryEntryToDisk(std::string_view path, const Dirent& dirent);
    void updateDirectoryEntryOnDisk(std::string_view path, const Dirent& dirent);
    void markDirectoryEntryAsDeleted(uint32_t dirCluster, std::string_view name);
    uint32_t findFreeDirEntrySlot(uint32_t dirCluster);
    uint32_t findFreeDirEntrySlots(uint32_t dirCluster, uint32_t count);
    void expandDirectoryIfNeeded(uint32_t dirCluster);
    
    // LFN support
    void writeLFNEntries(uint32_t dirCluster, uint32_t slotOffset, const std::string& longName, uint8_t checksum);
    std::vector<std::string> splitLFNChunks(const std::string& longName);
    uint8_t calculateLFNChecksum(const std::string& shortName);
    uint8_t calculateLFNChecksumForShortName(const std::string& longName);
    std::string createShortName(const std::string& longName) const;
    
    // Path lookup helpers
    std::string findPathInDirectory(const std::string& parentPath, const std::string& name) const;
    std::string toShortNamePath(const std::string& path) const;
    std::string resolvePath(std::string_view path) const;
};

#endif // FAT32VOLUME_H
