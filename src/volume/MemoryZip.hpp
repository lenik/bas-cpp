#ifndef MEMORYZIP_H
#define MEMORYZIP_H

#include "Volume.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"
#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

/**
 * MemoryZip provides read-only access to a ZIP archive embedded in memory
 */
class MemoryZip : public Volume {
private:
    const uint8_t* m_data;
    size_t m_size;
    
    // ZIP parsing structures
    struct ZipEntry {
        std::string name;
        size_t localHeaderOffset;
        size_t compressedSize;
        size_t uncompressedSize;
        uint16_t compressionMethod;
        bool isDirectory;
    };
    
    std::map<std::string, ZipEntry> m_entries; // Use map for faster lookup
    
public:
    MemoryZip(const uint8_t* data, size_t length);
    
    // Volume interface
    std::string getClass() const override { return "mz"; }
    std::string getId() const override;
    VolumeType getType() const override { return VolumeType::ARCHIVE; }
    bool isLocal() const override { return false; }
    void setLabel(std::string_view) override {}
    
    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, FileStatus* status) const override;
    
    void readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, //
        std::string_view path, bool recursive = false) override;
    
    bool createDirectory(std::string_view path) override;
    bool removeDirectory(std::string_view path) override;
    
    // std::unique_ptr<IReadStream> openForRead(std::string_view path, std::string_view encoding = "UTF-8") override;
    // std::unique_ptr<IWriteStream> openForWrite(std::string_view path, bool append = false, std::string_view encoding = "UTF-8") override;
    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path, bool append = false) override;
    std::unique_ptr<Reader> newReader(std::string_view path, std::string_view encoding = "UTF-8") override;
    std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false, std::string_view encoding = "UTF-8") override;

    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;
    std::unique_ptr<RandomReader> newRandomReader(std::string_view path, std::string_view encoding = "UTF-8") override;
    
    void writeFile(std::string_view path, const std::vector<uint8_t>& data) override;
    
    std::string getLocalFile(std::string_view path) const override;
    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.", std::string_view suffix = "") override;
    
    std::vector<uint8_t> readFile(std::string_view path) override;

protected:
    std::string getDefaultLabel() const override;
    void removeFileUnchecked(std::string_view path) override;
    void copyFileUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileUnchecked(std::string_view oldPath, std::string_view newPath) override;

private:
    void parseZip();
    const ZipEntry* findEntry(const std::string& path) const;
    std::vector<uint8_t> decompressEntry(const ZipEntry& entry) const;
    
};

#endif // MEMORYZIP_H
