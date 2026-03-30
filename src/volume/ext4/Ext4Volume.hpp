#ifndef EXT4VOLUME_H
#define EXT4VOLUME_H

#include "IUserDb.hpp"

#include "../Volume.hpp"

#include <cstdint>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

class Ext4Volume : public Volume {
private:
    struct Node {
        bool isDirectory = false;
        uint64_t size = 0;
        uint32_t inode = 0;
        uint16_t mode = 0;
        uint32_t uid = 0;
        uint32_t gid = 0;
        time_t atime = 0;
        time_t mtime = 0;
        time_t ctime = 0;
    };

    std::string m_imagePath;
    mutable std::unordered_map<std::string, Node> m_nodes; // path -> inode metadata cache
    mutable std::unordered_map<uint32_t, std::unordered_map<std::string, Node>>
        m_dirIndexCache; // dir inode -> direct children
    IUserDb* m_userDb = nullptr;
    int m_contextUid = 0;
    int m_contextGid = 0;
    std::vector<int> m_contextGroupIds;

public:
    explicit Ext4Volume(std::string_view imagePath);
    ~Ext4Volume() override;

    void setUserDb(IUserDb* userDb);
    void setContextUidGid(int uid, int gid);
    int getContextUid() const { return m_contextUid; }
    int getContextGid() const { return m_contextGid; }

    std::string getClass() const override { return "ext"; }
    std::string getId() const override { return m_imagePath; }
    VolumeType getType() const override { return VolumeType::ARCHIVE; }
    std::string getSource() const override;
    bool isLocal() const override { return false; }
    std::string getLocalFile(std::string_view path) const override;
    
    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, FileStatus* status) const override;
    void readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path,
                         bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path, bool append = false) override;
    std::unique_ptr<Reader> newReader(std::string_view path, std::string_view encoding = "UTF-8") override;
    std::unique_ptr<Writer> newWriter(std::string_view path, bool append = false,
                                      std::string_view encoding = "UTF-8") override;

    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;
    std::unique_ptr<RandomReader> newRandomReader(std::string_view path,
                                                  std::string_view encoding = "UTF-8") override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.", std::string_view suffix = "") override;

protected:
    std::string getDefaultLabel() const override;

    std::vector<uint8_t> readFileUnchecked(std::string_view path) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;

    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) override;

private:
    std::string normalizePath(std::string_view path) const;
    void buildIndex();
    void refreshContextGroups();
    bool resolveNode(std::string_view path, Node* out) const;
    const std::unordered_map<std::string, Node>& getDirectoryEntries(uint32_t inode) const;
    bool hasGroup(uint32_t gid) const;
    bool checkMode(const Node& node, int needMask) const;
};

#endif // EXT4VOLUME_H
