#ifndef EXT4VOLUME_H
#define EXT4VOLUME_H

#include "IUserDb.hpp"

#include "../Volume.hpp"

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

typedef std::vector<uint8_t> ByteArray;

class Ext4Volume : public Volume {
  public:
    struct Inode {
        bool isDirectory = false;
        uint64_t size = 0;
        ino_t ino = 0;
        uint16_t mode = 0;
        uint32_t uid = 0;
        uint32_t gid = 0;
        time_t atime = 0;
        time_t mtime = 0;
        time_t ctime = 0;

        Inode() = default;
        Inode(bool isDirectory, uint64_t size, ino_t ino, uint16_t mode, uint32_t uid,
              uint32_t gid, time_t atime, time_t mtime, time_t ctime)
            : isDirectory(isDirectory), size(size), ino(ino), mode(mode), uid(uid), gid(gid),
              atime(atime), mtime(mtime), ctime(ctime) {}
    };

    struct RtNode;
    using RtNodeObj = std::shared_ptr<RtNode>;
    using RtNodeRef = std::weak_ptr<RtNode>;

    // Runtime node for directory traversal
    struct RtNode : public Inode {
        RtNodeRef parent;
        std::unordered_map<std::string, RtNodeRef> children;

        RtNode() = default;
        RtNode(bool isDirectory, uint64_t size, ino_t ino, uint16_t mode, uint32_t uid,
               uint32_t gid, time_t atime, time_t mtime, time_t ctime)
            : Inode(isDirectory, size, ino, mode, uid, gid, atime, mtime, ctime) {}
    };

private:
    std::string m_imagePath;
    mutable std::unordered_map<std::string, ino_t> m_files; // normalized-path -> ino
    mutable std::unordered_map<ino_t, Inode> m_nodes;
    mutable std::unordered_map<ino_t, RtNodeObj> m_rtnodes;
    mutable std::unordered_map<ino_t, ByteArray> m_mem; // ino -> file content cache
    
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
    std::string normalizePath(std::string_view path) const;
    void buildIndex();
    void refreshContextGroups();
    bool resolveNode(std::string_view path, Inode* out) const;
    const RtNode* getDirectoryEntries(uint32_t inode) const;
    bool hasGroup(uint32_t gid) const;
    bool checkMode(const Inode& node, int needMask) const;

    // Write support
    uint32_t resolveParentInode(std::string_view path) const;
    std::string getBaseName(std::string_view path) const;
    void invalidateCache(std::string_view path);
};

#endif // EXT4VOLUME_H
