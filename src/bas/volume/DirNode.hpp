#ifndef FILESTATUS_H
#define FILESTATUS_H

#include "DirEntry.hpp"

#include <cstdint>
#include <ctime>
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

    ~DirNode() {
        if (target_owned) {
            if (target) {
                delete target;
                target = nullptr;
            }
        }
    }

  public:
    inline FileType targetType() const { return target ? target->type : FileType::Unknown; }

    inline DirNode* root() { return parent ? parent->root() : this; }
    inline size_t childCount() const { return children.size(); }

    inline bool isDirectoryTarget() const {
        if (isSymbolicLink())
            return target ? target->isDirectory() : false;
        else
            return isDirectory();
    }
    inline bool isFileTarget() const {
        if (isSymbolicLink())
            return target ? target->isRegularFile() : false;
        else
            return isRegularFile();
    }

    inline std::string path() const {
        std::string path;
        buildPath(path);
        return path;
    }

    inline DirNode* findChild(std::string_view name) { return findChild(std::string(name)); }
    inline DirNode* findChild(const std::string& name) {
        auto it = children.find(name);
        return it != children.end() ? it->second.get() : nullptr;
    }

    inline DirNode* addChild(std::string_view name, FileType type = FileType::Unknown,
                             int64_t size = 0, int64_t epochNano = 0) {
        return addChild(std::string(name), type, size, epochNano);
    }
    inline DirNode* addChild(const std::string& name, FileType type = FileType::Unknown,
                             int64_t size = 0, int64_t epochNano = 0) {
        auto child = std::make_unique<DirNode>(name, type, size, epochNano);
        child->parent = this;
        children[name] = std::move(child);
        return children.at(name).get();
    }

    inline DirNode* resolve(std::string_view path, bool create = false,
                            FileType intermediateType = FileType::Unknown) {
        size_t slash = path.find('/');
        if (slash == std::string_view::npos) {
            return findChild(path);
        }

        std::string_view tail = path.substr(slash + 1);
        bool last = tail.empty();

        std::string_view head = path.substr(0, slash);
        DirNode* child = findChild(head);
        if (!child) {
            if (create) {
                child = addChild(head, last ? FileType::Unknown : intermediateType);
            } else {
                return nullptr;
            }
        }
        if (last)
            return child;
        return child->resolve(tail, create, intermediateType);
    }

    inline int64_t modifiedNano() const { return epochNano; }
    inline int64_t accessNano() const { return accessEpochNano; }
    inline int64_t creationNano() const { return creationEpochNano; }

    inline int64_t modifiedSeconds() const { return epochNano / 1'000'000'000LL; }
    inline int64_t accessSeconds() const { return accessEpochNano / 1'000'000'000LL; }
    inline int64_t creationSeconds() const { return creationEpochNano / 1'000'000'000LL; }

    inline std::chrono::system_clock::time_point modifiedTime() const {
        return std::chrono::system_clock::time_point(std::chrono::nanoseconds(epochNano));
    }
    inline std::chrono::system_clock::time_point accessTime() const {
        return std::chrono::system_clock::time_point(std::chrono::nanoseconds(accessEpochNano));
    }
    inline std::chrono::system_clock::time_point creationTime() const {
        return std::chrono::system_clock::time_point(std::chrono::nanoseconds(creationEpochNano));
    }

    inline auto modifiedZonedTime() const {
        return date::zoned_time{date::current_zone(), modifiedTime()};
    }
    inline auto accessZonedTime() const {
        return date::zoned_time{date::current_zone(), accessTime()};
    }
    inline auto creationZonedTime() const {
        return date::zoned_time{date::current_zone(), creationTime()};
    }

    inline auto modifiedLocalTime() const { return modifiedZonedTime().get_local_time(); }
    inline auto accessLocalTime() const { return accessZonedTime().get_local_time(); }
    inline auto creationLocalTime() const { return creationZonedTime().get_local_time(); }

    inline void modifiedTime(std::chrono::system_clock::duration duration) {
        epochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    inline void modifiedTime(std::chrono::system_clock::time_point time) {
        modifiedTime(time.time_since_epoch());
    }

    inline void accessTime(std::chrono::system_clock::duration duration) {
        accessEpochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    inline void accessTime(std::chrono::system_clock::time_point time) {
        accessTime(time.time_since_epoch());
    }

    inline void creationTime(std::chrono::system_clock::duration duration) {
        creationEpochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }
    inline void creationTime(std::chrono::system_clock::time_point time) {
        creationTime(time.time_since_epoch());
    }

    inline void modifiedZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
        modifiedTime(zoned.get_sys_time());
    }
    inline void accessZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
        accessTime(zoned.get_sys_time());
    }
    inline void creationZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
        creationTime(zoned.get_sys_time());
    }

    inline void modifiedLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
        modifiedZonedTime(date::zoned_time{date::current_zone(), local});
    }
    inline void accessLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
        accessZonedTime(date::zoned_time{date::current_zone(), local});
    }
    inline void creationLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
        creationZonedTime(date::zoned_time{date::current_zone(), local});
    }

  private:
    void buildPath(std::string& path) const {
        if (parent)
            parent->buildPath(path);
        path += "/" + name;
    }
};

#endif // VSTATS_H
