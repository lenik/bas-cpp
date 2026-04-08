#ifndef ZIPVOLUME_H
#define ZIPVOLUME_H

#include "../BlockDevice.hpp"
#include "../MountOptions.hpp"
#include "../Volume.hpp"

#include "../../io/InputStream.hpp"
#include "../../io/OutputStream.hpp"
#include "../../io/Reader.hpp"
#include "../../io/Writer.hpp"

#include <cstddef>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct ZipOptions : public MountOptions {
    
};

// ZIP parsing structures
struct ZipEntry {
    std::string name;
    size_t localHeaderOffset;
    size_t compressedSize;
    size_t uncompressedSize;
    uint16_t compressionMethod;
    bool isDirectory;
    /** From central directory (DOS or UT extra); 0 if unknown. */
    time_t modifiedTime = 0;
    /** From UT extra (0x5455) atime; 0 if absent. */
    time_t accessTime = 0;
    /** From UT extra ctime; 0 if absent. */
    time_t creationTime = 0;
    /** From Info-ZIP "New Unix" extra (0x7875), if present. */
    bool hasUnixIds = false;
    unsigned int uid = 0;
    unsigned int gid = 0;
};

/**
 * ZipVolume provides read-only access to a ZIP archive embedded in memory
 */
class ZipVolume : public Volume {
  private:
    std::shared_ptr<BlockDevice> m_device;
    ZipOptions m_options;

    std::map<std::string, ZipEntry> m_entries; // Use map for faster lookup

  public:
    ZipVolume(std::shared_ptr<BlockDevice> device, const ZipOptions& options);

    // Volume interface
    std::string getClass() const override;
    std::string getUrl() const override;
    std::string getDeviceUrl() const override;
    VolumeType getType() const override;
    bool isLocal() const override { return false; }
    void setLabel(std::string_view) override {}

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, DirNode* status) const override;

    void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) override;

    // std::unique_ptr<IReadStream> openForRead(std::string_view path, std::string_view encoding =
    // "UTF-8") override; std::unique_ptr<IWriteStream> openForWrite(std::string_view path, bool
    // append = false, std::string_view encoding = "UTF-8") override;
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

    std::string getLocalFile(std::string_view path) const override;
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
    void parseZip();
    const ZipEntry* findEntry(const std::string& path) const;
    std::vector<uint8_t> decompressEntry(const ZipEntry& entry) const;
};

#endif // ZIPVOLUME_H
