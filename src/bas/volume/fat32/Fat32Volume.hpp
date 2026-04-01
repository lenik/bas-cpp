#ifndef FAT32VOLUME_H
#define FAT32VOLUME_H

#include "../Volume.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

class Fat32Volume : public Volume {
  private:
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

    std::string m_imagePath;
    uint64_t m_imageSize = 0;

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
    explicit Fat32Volume(std::string_view imagePath);

    std::string getClass() const override { return "fat32"; }
    std::string getId() const override { return m_imagePath; }
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
    uint64_t clusterToOffset(uint32_t cluster) const;
    bool readAt(uint64_t offset, uint8_t* dst, size_t len) const;

    std::string normalizePath(std::string_view path) const;
    std::string decodeShortName(const uint8_t* entry) const;
    std::string decodeLfnChunk(const uint8_t* entry) const;
};

#endif // FAT32VOLUME_H
