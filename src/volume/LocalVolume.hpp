#ifndef LOCALVOLUME_H
#define LOCALVOLUME_H

#include "Volume.hpp"
#include "volume/mountinfo.hpp"

#include <optional>
#include <string>

enum class LocalLogicalType {
    NONE,
    BIND,
    OVERLAY,
};

/**
 * Local filesystem volume implementation
 */
class LocalVolume : public Volume {
private:
    std::string m_rootPath;
    std::string m_label;
    mutable std::string m_cachedUUID;
    mutable std::string m_cachedLabel;
    mutable std::optional<std::string> m_mountPoint;
    mutable std::optional<std::string> m_device;
    mutable VolumeType m_type = VolumeType::OTHER;
    mutable LocalLogicalType m_logicalType = LocalLogicalType::NONE;
    mutable bool m_isLoop = false;
    mutable bool m_readOnly = false;
    mutable bool m_mountInfoCached = false;
    mutable MountInfo m_mountInfo;
    
#if defined(__linux__)
    bool isMountPoint(const std::string& path) const;
    std::string getMountDevice(const std::string& mountPoint) const;
    std::string getFilesystemUUID(const std::string& device) const;
    std::string getFilesystemLabel(const MountInfo& proc) const;
    void cacheMountInfo() const;
#elif defined(WINDOWS)

#endif

public:
    explicit LocalVolume(std::string_view rootPath);
    virtual ~LocalVolume() = default;
    
    std::string_view getRootPath() const { return m_rootPath; }
    void setRootPath(std::string_view rootPath);

    std::string getClass() const override { return "local"; }
    std::string getId() const override { return m_rootPath; }
    VolumeType getType() const override;
    bool isLocal() const override { return true; }
    std::string getLocalFile(std::string_view path) const override { return normalize(path); }
    std::string getUUID() override;
    std::string getSerial() override;
    std::string getLabel() override;
    void setLabel(std::string_view label) override;
    const std::optional<std::string>& getMountPoint() const;
    const std::optional<std::string>& getDevice() const;

    bool isLoop() const;
    LocalLogicalType getLogicalType() const;
    bool isLogical() const;
    bool isReadOnly() const;
    MountInfo getMountInfo() const;
    
    // Volume interface implementation
    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, FileStatus* status) const override;
    
    void readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path, bool recursive = false) override;
    
    // std::unique_ptr<IReadStream> openForRead(std::string_view path, std::string_view encoding = "UTF-8") override;
    // std::unique_ptr<IWriteStream> openForWrite(std::string_view path, bool append = false, std::string_view encoding = "UTF-8") override;
    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path, bool append = false) override;
    std::unique_ptr<Reader> newReader(std::string_view path, std::string_view encoding = "UTF-8") override;
    std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false, std::string_view encoding = "UTF-8") override;

    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;
    std::unique_ptr<RandomReader> newRandomReader(std::string_view path, std::string_view encoding = "UTF-8") override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.", std::string_view suffix = "") override;

protected:
    std::string getDefaultLabel() const override;

    std::string resolveLocal(std::string_view path) const;
    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) override;
    
    std::vector<uint8_t> readFileUnchecked(std::string_view path) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;
    
};

#endif // LOCALVOLUME_H
