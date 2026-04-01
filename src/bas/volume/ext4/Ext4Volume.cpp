#include "Ext4Volume.hpp"

#include "Ext4FileInputStream.hpp"
#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include <sys/stat.h>

namespace {
constexpr uint32_t kRootInode = 2;
}

Ext4Volume::Ext4Volume(std::string_view imagePath) : m_imagePath(imagePath) {
    if (m_imagePath.empty()) {
        throw std::invalid_argument("Ext4Volume: image path is required");
    }
    buildIndex();
}

Ext4Volume::~Ext4Volume() = default;

void Ext4Volume::setUserDb(IUserDb* userDb) {
    m_userDb = userDb;
    refreshContextGroups();
}

void Ext4Volume::setContextUidGid(int uid, int gid) {
    m_contextUid = uid;
    m_contextGid = gid;
    refreshContextGroups();
}

void Ext4Volume::refreshContextGroups() {
    m_contextGroupIds.clear();
    m_contextGroupIds.push_back(m_contextGid);
    if (m_userDb) {
        auto groups = m_userDb->getGroupIds(m_contextUid);
        m_contextGroupIds.insert(m_contextGroupIds.end(), groups.begin(), groups.end());
    }
}

std::string Ext4Volume::getDefaultLabel() const { return "EXT Image"; }

std::string Ext4Volume::getLocalFile(std::string_view /*path*/) const { return ""; }

std::string Ext4Volume::getSource() const { return "ext4 " + m_imagePath; }

bool Ext4Volume::exists(std::string_view path) const {
    Inode node;
    return resolveNode(path, &node);
}

bool Ext4Volume::isFile(std::string_view path) const {
    Inode node;
    return resolveNode(path, &node) && !node.isDirectory;
}

bool Ext4Volume::isDirectory(std::string_view path) const {
    Inode node;
    return resolveNode(path, &node) && node.isDirectory;
}

bool Ext4Volume::stat(std::string_view path, DirNode* status) const {
    if (!status) {
        throw std::invalid_argument("Ext4Volume::stat: status is null");
    }
    Inode node;
    const std::string normalized = normalizeArg(path);
    if (!resolveNode(normalized, &node)) {
        return false;
    }
    status->name = normalized;
    status->type = node.isDirectory ? FileType::Directory : FileType::Regular;
    status->size = node.size;
    status->ino = node.ino;
    status->mode = node.mode;
    status->uid = node.uid;
    status->gid = node.gid;
    status->epochSeconds(node.mtime);
    status->accessTime(std::chrono::system_clock::from_time_t(node.atime));
    status->creationTime(std::chrono::system_clock::from_time_t(node.ctime));
    return true;
}

void Ext4Volume::readDir_inplace(DirNode& context, std::string_view path, bool recursive) {
    const std::string parent = normalizeArg(path);
    Inode parentNode;
    if (!resolveNode(parent, &parentNode) || !parentNode.isDirectory) {
        throw IOException("readDir", std::string(path), "Path is not a directory");
    }
    if (!checkMode(parentNode, 4 | 1)) { // r+x
        throw IOException("readDir", std::string(path), "Permission denied");
    }

    const auto& rtnode = getDirectoryEntries(parentNode.ino);
    if (!recursive) {
        for (const auto& [name, child] : rtnode->children) {
            auto st = std::make_unique<DirNode>();
            st->name = name;
            st->type = child->isDirectory ? FileType::Directory : FileType::Regular;
            st->size = child->size;
            st->ino = child->ino;
            st->mode = child->mode;
            st->uid = child->uid;
            st->gid = child->gid;
            st->epochSeconds(child->mtime);
            st->accessTime(std::chrono::system_clock::from_time_t(child->atime));
            st->creationTime(std::chrono::system_clock::from_time_t(child->ctime));
            context.putChild(std::move(st));
        }
        return;
    }

    std::unordered_set<uint32_t> visitedDirs;
    std::function<void(const std::string&, const Inode&)> walk = [&](const std::string& basePath,
                                                                     const Inode& inode) {
        if (!visitedDirs.insert(inode.ino).second) {
            return;
        }
        const auto& rtnode = getDirectoryEntries(inode.ino);
        for (const auto& [name, child] : rtnode->children) {
            std::string absPath = (basePath == "/") ? ("/" + name) : (basePath + "/" + name);
            std::string relPath =
                (parent == "/") ? absPath.substr(1) : absPath.substr(parent.size() + 1);

            auto st = std::make_unique<DirNode>();
            st->name = relPath;
            st->type = child->isDirectory ? FileType::Directory : FileType::Regular;
            st->size = child->size;
            st->ino = child->ino;
            st->mode = child->mode;
            st->uid = child->uid;
            st->gid = child->gid;
            st->epochSeconds(child->mtime);
            st->accessTime(std::chrono::system_clock::from_time_t(child->atime));
            st->creationTime(std::chrono::system_clock::from_time_t(child->ctime));
            context.putChild(std::move(st));

            m_inodes[absPath] = *child;
            if (child->isDirectory) {
                walk(absPath, *child);
            }
        }
    };
    walk(parent, parentNode);
}

std::unique_ptr<InputStream> Ext4Volume::newInputStream(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    Inode node;
    if (!resolveNode(normalized, &node) || node.isDirectory) {
        throw IOException("newInputStream", std::string(path), "File not found or is a directory");
    }
    if (!checkMode(node, 4)) {
        throw IOException("newInputStream", std::string(path), "Permission denied");
    }
    return std::make_unique<Ext4FileInputStream>(m_imagePath, node.ino);
}

std::unique_ptr<RandomInputStream> Ext4Volume::newRandomInputStream(std::string_view path) {
    auto in = newInputStream(path);
    return std::unique_ptr<RandomInputStream>(dynamic_cast<RandomInputStream*>(in.release()));
}

std::unique_ptr<Reader> Ext4Volume::newReader(std::string_view path,
                                              std::string_view /*encoding*/) {
    auto bytes = readFile(path);
    return std::make_unique<StringReader>(std::string(bytes.begin(), bytes.end()));
}

std::unique_ptr<RandomReader> Ext4Volume::newRandomReader(std::string_view path,
                                                          std::string_view encoding) {
    return Volume::newRandomReader(path, encoding);
}

std::unique_ptr<OutputStream> Ext4Volume::newOutputStream(std::string_view path, bool append) {
    return std::make_unique<Ext4FileOutputStream>(m_imagePath, std::string(path), append);
}

std::unique_ptr<Writer> Ext4Volume::newWriter(std::string_view path, bool /*append*/,
                                              std::string_view /*encoding*/) {
    throw IOException("newWriter", std::string(path),
                      "Ext4Volume write operations are not implemented yet");
}

std::string Ext4Volume::getTempDir() {
    throw IOException("getTempDir", "", "Ext4Volume does not support temp file operations");
}

std::string Ext4Volume::createTempFile(std::string_view /*prefix*/, std::string_view /*suffix*/) {
    throw IOException("createTempFile", "", "Ext4Volume does not support temp file operations");
}

std::vector<uint8_t> Ext4Volume::readFileUnchecked(std::string_view path, int64_t off, size_t len) {
    const std::string normalized = normalizeArg(path);
    Inode node;
    if (!resolveNode(normalized, &node) || node.isDirectory) {
        throw IOException("readFile", std::string(path), "File not found or is a directory");
    }
    if (!checkMode(node, 4)) {
        throw IOException("readFile", std::string(path), "Permission denied");
    }

    auto in = newInputStream(path);
    if (!in)
        return {};
    auto data = in->readBytesUntilEOF();
    if (data.size() > node.size)
        data.resize(static_cast<size_t>(node.size));

    int64_t start64 = (off >= 0) ? off : (static_cast<int64_t>(data.size()) + off + 1);
    if (start64 < 0) {
        start64 = 0;
    }
    size_t start = static_cast<size_t>(start64);
    if (start >= data.size()) {
        return {};
    }

    size_t end = data.size();
    if (len > 0) {
        end = std::min(end, start + len);
    }

    return std::vector<uint8_t>(data.begin() + start, data.begin() + end);
}

void Ext4Volume::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& /*data*/) {
    throw IOException("writeFile", std::string(path),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::createDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("createDirectory", std::string(path),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::removeDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("removeDirectory", std::string(path),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::removeFileThrowsUnchecked(std::string_view path) {
    throw IOException("removeFile", std::string(path),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::copyFileThrowsUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("copyFile", std::string(src),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::moveFileThrowsUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("moveFile", std::string(src),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view /*newPath*/) {
    throw IOException("renameFile", std::string(oldPath),
                      "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::buildIndex() {
    // Rebuild lightweight caches only. Full tree scan is intentionally avoided.
    m_inodes.clear();
    m_rtnodes.clear();

    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }
    struct ext2_inode rootIno{};
    if (ext2fs_read_inode(fs, kRootInode, &rootIno) == 0) {
        m_inodes["/"] = Inode{true,
                              0,
                              kRootInode,
                              static_cast<uint16_t>(rootIno.i_mode),
                              rootIno.i_uid,
                              rootIno.i_gid,
                              static_cast<time_t>(rootIno.i_atime),
                              static_cast<time_t>(rootIno.i_mtime),
                              static_cast<time_t>(rootIno.i_ctime)};
    } else {
        m_inodes["/"] =
            Inode{true, 0, kRootInode, static_cast<uint16_t>(S_IFDIR | 0755), 0, 0, 0, 0, 0};
    }
    ext2fs_close(fs);
}

bool Ext4Volume::resolveNode(std::string_view path, Inode* out) const {
    const std::string normalized = normalizeArg(path);
    auto cached = m_inodes.find(normalized);
    if (cached != m_inodes.end()) {
        if (out)
            *out = cached->second;
        return true;
    }
    if (normalized == "/") {
        if (out)
            *out = m_inodes.at("/");
        return true;
    }

    Inode cur = m_inodes.at("/");
    std::string curPath = "/";

    size_t start = 1;
    while (start <= normalized.size()) {
        size_t slash = normalized.find('/', start);
        std::string part = normalized.substr(start, slash == std::string::npos ? std::string::npos
                                                                               : slash - start);
        if (part.empty())
            break;

        if (!checkMode(cur, 1)) { // execute/search on current directory
            return false;
        }
        const auto& rtnode = getDirectoryEntries(cur.ino);
        auto it = rtnode->children.find(part);
        if (it == rtnode->children.end()) {
            return false;
        }
        cur = *it->second;
        curPath = (curPath == "/") ? ("/" + part) : (curPath + "/" + part);
        m_inodes[curPath] = cur;

        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    if (curPath != normalized) {
        return false;
    }
    if (out)
        *out = cur;
    return true;
}

const Ext4Volume::RtNode* Ext4Volume::getDirectoryEntries(uint32_t inode) const {
    auto itCache = m_rtnodes.find(inode);
    if (itCache != m_rtnodes.end()) {
        return itCache->second.get();
    }

    auto rtnode = std::make_unique<RtNode>();

    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }

    struct Scope {
        ext2_filsys fs;
        RtNode* rtnode;
    } scope{fs, rtnode.get()};

    ext2fs_dir_iterate2(
        fs, static_cast<ext2_ino_t>(inode), 0, nullptr,
        [](ext2_ino_t, int, struct ext2_dir_entry* dirent, int, int, char*, void* priv) -> int {
            Scope* scope = static_cast<Scope*>(priv);
            if (!dirent || dirent->inode == 0)
                return 0;
            std::string name(dirent->name, dirent->name + dirent->name_len);
            if (name == "." || name == "..")
                return 0;

            struct ext2_inode inodeBuf{};
            if (ext2fs_read_inode(scope->fs, dirent->inode, &inodeBuf) != 0)
                return 0;
            const bool isDir = LINUX_S_ISDIR(inodeBuf.i_mode);
            const uint64_t size = EXT2_I_SIZE(&inodeBuf);
            RtNode* ctx = scope->rtnode;
            auto child = std::make_unique<RtNode>(
                isDir, size, dirent->inode, inodeBuf.i_mode, inodeBuf.i_uid, inodeBuf.i_gid,
                static_cast<time_t>(inodeBuf.i_atime), static_cast<time_t>(inodeBuf.i_mtime),
                static_cast<time_t>(inodeBuf.i_ctime));
            ctx->children[name] = std::move(child);
            return 0;
        },
        &scope);

    ext2fs_close(fs);

    auto inserted = m_rtnodes.emplace(inode, std::move(rtnode));
    return inserted.first->second.get();
}

bool Ext4Volume::hasGroup(uint32_t gid) const {
    for (int g : m_contextGroupIds) {
        if (g == static_cast<int>(gid)) {
            return true;
        }
    }
    return false;
}

bool Ext4Volume::checkMode(const Inode& node, int needMask) const {
    // root bypass
    if (m_contextUid == 0) {
        return true;
    }

    int shift = 0;
    if (node.uid == static_cast<uint32_t>(m_contextUid)) {
        shift = 6;
    } else if (node.gid == static_cast<uint32_t>(m_contextGid) || hasGroup(node.gid)) {
        shift = 3;
    } else {
        shift = 0;
    }

    const int perms = (node.mode >> shift) & 0x7;
    return (perms & needMask) == needMask;
}
