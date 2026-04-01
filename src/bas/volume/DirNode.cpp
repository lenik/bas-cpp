#include "DirNode.hpp"

#include <chrono>
#include <ctime>

static auto timestamp(struct statx_timestamp tv) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(tv.tv_sec) +
                                                 std::chrono::nanoseconds(tv.tv_nsec));
}

static auto tv_sec(time_t tv_sec) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(tv_sec));
}

#if defined(__linux__)
void DirNode::assign(const struct statx& sx) {
    type = S_ISDIR(sx.stx_mode) ? FileType::Directory : FileType::Regular;
    if (S_ISREG(sx.stx_mode))
        size = sx.stx_size;
    else
        size = 0;
    ino = sx.stx_ino;
    modifiedTime(timestamp(sx.stx_mtime));

    accessTime(timestamp(sx.stx_atime));
    if (sx.stx_mask & STATX_BTIME)
        creationTime(timestamp(sx.stx_btime));
    else
        creationTime(timestamp(sx.stx_mtime));

    mode = sx.stx_mode;
    uid = sx.stx_uid;
    gid = sx.stx_gid;
    nlink = sx.stx_nlink;
    dev = sx.stx_dev_major << 8 | sx.stx_dev_minor;
    blksize = sx.stx_blksize;
    blocks = sx.stx_blocks;
}
#endif

void DirNode::assign(const struct stat& sb) {
    type = S_ISDIR(sb.st_mode) ? FileType::Directory : FileType::Regular;
    if (S_ISREG(sb.st_mode))
        size = static_cast<uint64_t>(sb.st_size);
    else
        size = 0;
    ino = sb.st_ino;
    modifiedTime(tv_sec(sb.st_mtime));

    accessTime(tv_sec(sb.st_atime));
    creationTime(tv_sec(sb.st_mtime));

    mode = sb.st_mode;
    uid = sb.st_uid;
    gid = sb.st_gid;
    nlink = sb.st_nlink;
    dev = sb.st_dev;
    blksize = sb.st_blksize;
    blocks = sb.st_blocks;
}

void DirNode::assign(const DirNode& node) {
    name = node.name;
    type = node.type;
    size = node.size;
    ino = node.ino;
    epochNano = node.epochNano;

    accessTime(node.accessTime());
    creationTime(node.creationTime());

    mode = node.mode;
    uid = node.uid;
    gid = node.gid;
    nlink = node.nlink;
    dev = node.dev;
    blksize = node.blksize;
    blocks = node.blocks;

    setTarget(node.target, false);

    invalidateChildren();
    invalidateCache();
}

void DirNode::clear() {
    DirEntry::clear();
    creationEpochNano = 0;
    accessEpochNano = 0;
    mode = 0;
    uid = 0;
    gid = 0;
    nlink = 0;
    dev = 0;
    blksize = 0;
    blocks = 0;
    target = nullptr;
    target_owned = false;
    parent = nullptr;
    cache.clear();
    cache_valid = false;
}

void DirNode::invalidateCache() {
    cache.clear();
    cache_valid = false;
}

void DirNode::invalidateChildren() {
    children.clear();
    children_valid = false;
}

void DirNode::setUnknownClear() {
    setUnknown();
    invalidateChildren();
    invalidateCache();
}

void DirNode::setRegularClear() {
    setRegular();
    invalidateChildren();
    invalidateCache();
}

void DirNode::setDirectoryClear() {
    setDirectory();
    invalidateChildren();
    invalidateCache();
}

void DirNode::setTarget(DirNode* target, bool owned) {
    if (this->target) {
        if (target_owned) {
            delete this->target;
            this->target = nullptr;
        }
    }
    this->target = target;
    target_owned = owned;
}

void DirNode::setSymbolicLink(DirNode* target, bool owned) {
    type = FileType::Symlink;
    setTarget(target, owned);
}

void DirNode::setSymbolicLinkClear(DirNode* target, bool owned) {
    setSymbolicLink(target, owned);
    invalidateChildren();
    invalidateCache();
}

FileType DirNode::targetType() const { return target ? target->type : FileType::Unknown; }

bool DirNode::isDirectoryTarget() const {
    if (isSymbolicLink())
        return target ? target->isDirectory() : false;
    else
        return isDirectory();
}
bool DirNode::isFileTarget() const {
    if (isSymbolicLink())
        return target ? target->isRegularFile() : false;
    else
        return isRegularFile();
}

int64_t DirNode::modifiedNano() const { return epochNano; }
int64_t DirNode::accessNano() const { return accessEpochNano; }
int64_t DirNode::creationNano() const { return creationEpochNano; }

int64_t DirNode::modifiedSeconds() const { return epochNano / 1'000'000'000LL; }
int64_t DirNode::accessSeconds() const { return accessEpochNano / 1'000'000'000LL; }
int64_t DirNode::creationSeconds() const { return creationEpochNano / 1'000'000'000LL; }

std::chrono::system_clock::time_point DirNode::modifiedTime() const {
    return std::chrono::system_clock::time_point(std::chrono::nanoseconds(epochNano));
}
std::chrono::system_clock::time_point DirNode::accessTime() const {
    return std::chrono::system_clock::time_point(std::chrono::nanoseconds(accessEpochNano));
}
std::chrono::system_clock::time_point DirNode::creationTime() const {
    return std::chrono::system_clock::time_point(std::chrono::nanoseconds(creationEpochNano));
}

auto DirNode::modifiedZonedTime() const {
    return date::zoned_time{date::current_zone(), modifiedTime()};
}
auto DirNode::accessZonedTime() const {
    return date::zoned_time{date::current_zone(), accessTime()};
}
auto DirNode::creationZonedTime() const {
    return date::zoned_time{date::current_zone(), creationTime()};
}

auto DirNode::modifiedLocalTime() const { return modifiedZonedTime().get_local_time(); }
auto DirNode::accessLocalTime() const { return accessZonedTime().get_local_time(); }
auto DirNode::creationLocalTime() const { return creationZonedTime().get_local_time(); }

void DirNode::modifiedTime(std::chrono::system_clock::duration duration) {
    epochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}
void DirNode::modifiedTime(std::chrono::system_clock::time_point time) {
    modifiedTime(time.time_since_epoch());
}

void DirNode::accessTime(std::chrono::system_clock::duration duration) {
    accessEpochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}
void DirNode::accessTime(std::chrono::system_clock::time_point time) {
    accessTime(time.time_since_epoch());
}

void DirNode::creationTime(std::chrono::system_clock::duration duration) {
    creationEpochNano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}
void DirNode::creationTime(std::chrono::system_clock::time_point time) {
    creationTime(time.time_since_epoch());
}

void DirNode::modifiedZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
    modifiedTime(zoned.get_sys_time());
}
void DirNode::accessZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
    accessTime(zoned.get_sys_time());
}
void DirNode::creationZonedTime(date::zoned_time<std::chrono::system_clock::duration> zoned) {
    creationTime(zoned.get_sys_time());
}

void DirNode::modifiedLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
    modifiedZonedTime(date::zoned_time{date::current_zone(), local});
}
void DirNode::accessLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
    accessZonedTime(date::zoned_time{date::current_zone(), local});
}
void DirNode::creationLocalTime(date::local_time<std::chrono::system_clock::duration> local) {
    creationZonedTime(date::zoned_time{date::current_zone(), local});
}

std::string DirNode::path() const {
    std::string path;
    buildPath(path);
    return path;
}

DirNode* DirNode::root() { return parent ? parent->root() : this; }
size_t DirNode::childCount() const { return children_valid ? children.size() : 0; }

DirNode* DirNode::findChild(std::string_view name) { return findChild(std::string(name)); }
DirNode* DirNode::findChild(const std::string& name) {
    auto it = children.find(name);
    return it != children.end() ? it->second.get() : nullptr;
}

int DirNode::walkRec(std::function<bool(DirNode*)> callback) { return walk(callback, true); }
int DirNode::walk(std::function<bool(DirNode*)> callback, bool recursive) {
    int call_count = 0;
    for (const auto& [name, child] : children) {
        if (!callback(child.get())) {
            return call_count;
        }
        if (recursive) {
            call_count += child->walk(callback, recursive);
        }
    }
    return call_count;
}

DirNode* DirNode::putChild(std::string_view name, FileType type, int64_t size, int64_t epochNano) {
    return putChild(std::string(name), type, size, epochNano);
}
DirNode* DirNode::putChild(const std::string& name, FileType type, int64_t size,
                           int64_t epochNano) {
    auto child = std::make_unique<DirNode>(name, type, size, epochNano);
    child->parent = this;
    children[name] = std::move(child);
    return children.at(name).get();
}

DirNode* DirNode::putChild(std::unique_ptr<DirNode> child) {
    child->parent = this;
    const std::string key = child->name;
    children[key] = std::move(child);
    return children.at(key).get();
}

DirNode* DirNode::resolve(std::string_view path, bool create, FileType intermediateType) {
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
            child = putChild(head, last ? FileType::Unknown : intermediateType);
        } else {
            return nullptr;
        }
    }
    if (last)
        return child;
    return child->resolve(tail, create, intermediateType);
}

void DirNode::buildPath(std::string& path) const {
    if (parent) {
        parent->buildPath(path);
        path += "/";
    }
    path += name;
}