#ifndef FILESTATUS_H
#define FILESTATUS_H

#include "DirEntry.hpp"

#include <cstdint>
#include <ctime>
#include <functional>
#include <unordered_map>

#if defined(__linux__)
#include <sys/stat.h>
#endif

struct DirNode : public DirEntry {
    int64_t creationEpochNano{0};
    int64_t accessEpochNano{0};

    uint16_t mode{0};
    uint32_t uid{0};
    uint32_t gid{0};

    uint64_t dev{0};
    uint32_t nlink{0};
    uint64_t rdev{0};
    uint64_t blksize{0};
    uint64_t blocks{0};

    DirNode* target{nullptr};
    bool target_owned{false};

    DirNode* parent{nullptr};
    std::unordered_map<std::string, std::unique_ptr<DirNode>> children;
    bool children_valid{false};

    std::vector<uint8_t> cache;
    bool cache_valid{false};

  public:
    DirNode() = default;

    DirNode(std::string_view name, FileType type = FileType::Unknown, int64_t size = 0,
            int64_t epochNano = 0)
        : DirEntry(name, type, size, epochNano), //
          creationEpochNano(epochNano), accessEpochNano(epochNano) {}

    DirNode(const DirEntry& ent)
        : DirEntry(ent), //
          creationEpochNano(ent.epochNano), accessEpochNano(ent.epochNano) {}

#if defined(__linux__)
    void assign(const struct statx& sx);
#endif
    void assign(const struct stat& sb);
    void assign(const DirNode& node);

    ~DirNode() {
        if (target_owned) {
            if (target) {
                delete target;
                target = nullptr;
            }
        }
    }

  public:
    void clear();
    void invalidateCache();
    void invalidateChildren();

    void setUnknownClear();
    void setRegularClear();
    void setDirectoryClear();
    void setTarget(DirNode* target, bool owned = false);

    void setSymbolicLink(DirNode* target, bool owned = false);
    void setSymbolicLinkClear(DirNode* target, bool owned = false);

    FileType targetType() const;
    bool isDirectoryTarget() const;
    bool isFileTarget() const;

    int64_t modifiedNano() const;
    int64_t accessNano() const;
    int64_t creationNano() const;

    int64_t modifiedSeconds() const;
    int64_t accessSeconds() const;
    int64_t creationSeconds() const;

    std::chrono::system_clock::time_point modifiedTime() const;
    std::chrono::system_clock::time_point accessTime() const;
    std::chrono::system_clock::time_point creationTime() const;

    auto modifiedZonedTime() const;
    auto accessZonedTime() const;
    auto creationZonedTime() const;

    auto modifiedLocalTime() const;
    auto accessLocalTime() const;
    auto creationLocalTime() const;

    void modifiedTime(std::chrono::system_clock::duration duration);
    void modifiedTime(std::chrono::system_clock::time_point time);

    void accessTime(std::chrono::system_clock::duration duration);
    void accessTime(std::chrono::system_clock::time_point time);

    void creationTime(std::chrono::system_clock::duration duration);
    void creationTime(std::chrono::system_clock::time_point time);

    void modifiedZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned);
    void accessZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned);
    void creationZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned);

    void modifiedLocalTime(date::local_time<std::chrono::system_clock::duration> local);
    void accessLocalTime(date::local_time<std::chrono::system_clock::duration> local);
    void creationLocalTime(date::local_time<std::chrono::system_clock::duration> local);

    std::string path() const;
    DirNode* root();
    size_t childCount() const;

    DirNode* findChild(std::string_view name);
    DirNode* findChild(const std::string& name);

    int walkRec(std::function<bool(DirNode*)> callback);
    int walk(std::function<bool(DirNode*)> callback, bool recursive = false);

    DirNode* putChild(std::string_view name, FileType type = FileType::Unknown, int64_t size = 0,
                      int64_t epochNano = 0);
    DirNode* putChild(const std::string& name, FileType type = FileType::Unknown, int64_t size = 0,
                      int64_t epochNano = 0);
    DirNode* putChild(std::unique_ptr<DirNode> child);

    DirNode* resolve(std::string_view path, bool create = false,
                     FileType intermediateType = FileType::Unknown);

  private:
    void buildPath(std::string& path) const;
};

#endif // VSTATS_H
