#ifndef VOLUMEFILE_H
#define VOLUMEFILE_H

#include "DirNode.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"
#include "../io/Reader.hpp"
#include "../io/Writer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Volume;

/**
 * VolumeFile - A wrapper around Volume that provides convenient file operations.
 */
struct VolumeFile {
    Volume* m_volume;
    std::string m_path;

  public:
    VolumeFile(Volume* volume, std::string path);
    VolumeFile(const VolumeFile& other) = default;
    VolumeFile(VolumeFile&& other) noexcept = default;
    VolumeFile& operator=(const VolumeFile& other) = default;
    VolumeFile& operator=(VolumeFile&& other) noexcept = default;
    ~VolumeFile() = default;

    bool operator==(const VolumeFile& other) const {
        return m_volume == other.m_volume && m_path == other.m_path;
    }
    bool operator!=(const VolumeFile& other) const { return !(*this == other); }

    Volume* getVolume() const { return m_volume; }
    void setVolume(Volume* volume);

    std::string getPath() const { return m_path; }
    void setPath(std::string path);

    std::string getLocalFile() const;

    // Helper methods for reading file content
    size_t cReadFile(uint8_t* buf, size_t off, size_t len, //
                     int64_t file_offset = 0, std::ios::seekdir seek_dir = std::ios::beg) const;
    size_t cWriteFile(const uint8_t* buf, size_t off, size_t len, bool append = false) const;

    std::vector<uint8_t>
    readFile(int64_t off = 0, size_t len = 0,
             std::optional<std::vector<uint8_t>> default_data = std::nullopt) const;

    std::string readFileUTF8(std::optional<std::string> default_data = std::nullopt) const;
    std::string readFileString(std::string_view encoding = "UTF-8",
                               std::optional<std::string> default_data = std::nullopt) const;

    std::vector<std::string> readFileLines(std::string_view encoding = "UTF-8") const;

    void writeFile(const std::vector<uint8_t>& data) const;
    void writeFileString(std::string_view data, std::string_view encoding = "UTF-8") const;
    void writeFileLines(const std::vector<std::string>& lines,
                        std::string_view encoding = "UTF-8") const;

    // Export virtual file to temporary local file (for media playback, etc.)
    // Returns the temporary file path, or empty string on failure
    // The temporary file should be deleted when no longer needed
    // Use deleteTempFile() to clean it up
    std::string exportToTempFile() const;

    // Check if a video file format requires temporary file (doesn't support streaming)
    // Returns true if format requires temp file (e.g., .wmv, .asf)
    bool requiresTempFileForPlayback(std::string_view fileName) const;

    // Resolve relative path and return new VolumeFile
    std::unique_ptr<VolumeFile> resolve(std::string_view relativePath) const;
    std::unique_ptr<VolumeFile> getParentFile() const;
    std::unique_ptr<VolumeFile> getRootFile() const;

    // Filesystem operations (delegated to volume)
    // Testing methods - don't throw exceptions
    bool exists() const;
    bool isFile() const;
    bool isDirectory() const;
    bool canRead() const;
    bool canWrite() const;

    // IO operations - can throw IOException or AccessException
    bool createDirectory() const;
    bool createDirectories() const;
    bool createParentDirectories() const;

    bool removeDirectory() const;
    bool removeFile() const;

    bool copyTo(std::string_view destPath, bool overwrite = true) const;
    bool moveTo(std::string_view destPath, bool overwrite = true) const;
    bool renameTo(std::string_view destPath, bool overwrite = true) const;

    // readDir and stat can throw IOException or AccessException
    void readDir(DirNode& context, bool recursive = false) const;
    std::unique_ptr<DirNode> readDir(bool recursive = false) const {
        auto context = std::make_unique<DirNode>();
        readDir(*context, recursive);
        return context;
    }
    bool stat(DirNode* status) const;

    // std::unique_ptr<IReadStream> openForRead() const;
    // std::unique_ptr<IWriteStream> openForWrite(bool append = false) const;
    std::unique_ptr<InputStream> newInputStream() const;
    std::unique_ptr<OutputStream> newOutputStream(bool append = false) const;
    std::unique_ptr<Reader> newReader(std::string_view encoding = "UTF-8") const;
    std::unique_ptr<Writer> newWriter(bool append = false,
                                      std::string_view encoding = "UTF-8") const;

    std::unique_ptr<RandomInputStream> newRandomInputStream() const;
    std::unique_ptr<RandomReader> newRandomReader(std::string_view encoding = "UTF-8") const;
};

#endif // VOLUMEFILE_H
