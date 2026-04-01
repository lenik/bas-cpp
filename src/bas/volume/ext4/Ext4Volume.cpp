#include "Ext4Volume.hpp"

#include "Ext4FileInputStream.hpp"
#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"
#include "../../io/PrintStream.hpp"

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

    context.clear();
    context.name = parent;
    context.setDirectoryClear();
    context.children_valid = true;

    auto appendChild = [&](std::string_view nodeName, const Inode& child) {
        auto st = std::make_unique<DirNode>();
        st->name = std::string(nodeName);
        st->type = child.isDirectory ? FileType::Directory : FileType::Regular;
        st->size = child.size;
        st->ino = child.ino;
        st->mode = child.mode;
        st->uid = child.uid;
        st->gid = child.gid;
        st->epochSeconds(child.mtime);
        st->accessTime(std::chrono::system_clock::from_time_t(child.atime));
        st->creationTime(std::chrono::system_clock::from_time_t(child.ctime));
        context.putChild(std::move(st));
    };

    const auto& rtnode = getDirectoryEntries(parentNode.ino);
    if (!recursive) {
        for (const auto& [name, childRef] : rtnode->children) {
            if (auto child = childRef.lock()) {
                appendChild(name, *child);
            }
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
        for (const auto& [name, childRef] : rtnode->children) {
            if (auto child = childRef.lock()) {
                std::string absPath = (basePath == "/") ? ("/" + name) : (basePath + "/" + name);
                std::string relPath =
                    (parent == "/") ? absPath.substr(1) : absPath.substr(parent.size() + 1);

                appendChild(relPath, *child);

                m_files[absPath] = child->ino;
                if (child->isDirectory) {
                    walk(absPath, *child);
                }
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

std::unique_ptr<OutputStream> Ext4Volume::newOutputStream(std::string_view path, bool append) {
    return std::make_unique<Ext4FileOutputStream>(m_imagePath, std::string(path), append);
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

void Ext4Volume::buildIndex() {
    // Rebuild lightweight caches only. Full tree scan is intentionally avoided.
    m_files.clear();
    m_nodes.clear();
    m_rtnodes.clear();
    m_mem.clear();

    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }
    struct ext2_inode rootIno{};
    if (ext2fs_read_inode(fs, kRootInode, &rootIno) == 0) {
        Inode root{true,
                   0,
                   kRootInode,
                   static_cast<uint16_t>(rootIno.i_mode),
                   rootIno.i_uid,
                   rootIno.i_gid,
                   static_cast<time_t>(rootIno.i_atime),
                   static_cast<time_t>(rootIno.i_mtime),
                   static_cast<time_t>(rootIno.i_ctime)};
        m_nodes[kRootInode] = root;
        m_files["/"] = kRootInode;
    } else {
        Inode root{true, 0, kRootInode, static_cast<uint16_t>(S_IFDIR | 0755), 0, 0, 0, 0, 0};
        m_nodes[kRootInode] = root;
        m_files["/"] = kRootInode;
    }
    ext2fs_close(fs);
}

bool Ext4Volume::resolveNode(std::string_view path, Inode* out) const {
    const std::string normalized = normalizeArg(path);
    if (normalized.empty() || normalized == "/") {
        if (out) {
            auto it = m_nodes.find(kRootInode);
            if (it != m_nodes.end()) {
                *out = it->second;
            }
        }
        return true;
    }

    // Try to resolve from path cache
    auto fit = m_files.find(normalized);
    if (fit != m_files.end()) {
        auto nit = m_nodes.find(fit->second);
        if (nit != m_nodes.end()) {
            if (out)
                *out = nit->second;
            return true;
        }
    }

    // Walk the path from root
    ino_t curIno = kRootInode;
    std::string curPath = "/";

    size_t start = 1;
    while (start < normalized.size()) {
        size_t slash = normalized.find('/', start);
        std::string part = normalized.substr(start, slash == std::string::npos ? std::string::npos
                                                                               : slash - start);
        if (part.empty()) {
            break;
        }

        auto nit = m_nodes.find(curIno);
        if (nit == m_nodes.end()) {
            return false;
        }
        if (!checkMode(nit->second, 1)) { // execute/search on current directory
            return false;
        }

        const auto& rtnode = getDirectoryEntries(curIno);
        auto it = rtnode->children.find(part);
        if (it == rtnode->children.end()) {
            return false;
        }

        if (auto child = it->second.lock()) {
            curIno = child->ino;
            curPath = (curPath == "/") ? ("/" + part) : (curPath + "/" + part);

            // Cache the resolved node
            m_nodes[curIno] = *child;
            m_files[curPath] = curIno;
        } else {
            return false;
        }

        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    if (curPath != normalized) {
        return false;
    }

    auto nit = m_nodes.find(curIno);
    if (nit == m_nodes.end()) {
        return false;
    }
    if (out)
        *out = nit->second;
    return true;
}

const Ext4Volume::RtNode* Ext4Volume::getDirectoryEntries(uint32_t inode) const {
    auto itCache = m_rtnodes.find(inode);
    if (itCache != m_rtnodes.end()) {
        return itCache->second.get();
    }

    auto rtnode = std::make_shared<RtNode>();

    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }

    struct Scope {
        ext2_filsys fs;
        RtNode* rtnode;
        const Ext4Volume* volume;
    } scope{fs, rtnode.get(), this};

    ext2fs_dir_iterate2(
        fs, static_cast<ext2_ino_t>(inode), 0, nullptr,
        [](ext2_ino_t, int, struct ext2_dir_entry* dirent, int, int, char*, void* priv) -> int {
            Scope* scope = static_cast<Scope*>(priv);
            if (!dirent || dirent->inode == 0)
                return 0;
            const int nameLen = ext2fs_dirent_name_len(dirent);
            if (nameLen <= 0) {
                return 0;
            }
            std::string name(dirent->name, dirent->name + nameLen);
            if (name == "." || name == "..")
                return 0;

            struct ext2_inode inodeBuf{};
            if (ext2fs_read_inode(scope->fs, dirent->inode, &inodeBuf) != 0)
                return 0;
            const bool isDir = LINUX_S_ISDIR(inodeBuf.i_mode);
            const uint64_t size = EXT2_I_SIZE(&inodeBuf);
            
            // Cache the inode
            Inode childInode{
                isDir, size, dirent->inode, inodeBuf.i_mode, inodeBuf.i_uid, inodeBuf.i_gid,
                static_cast<time_t>(inodeBuf.i_atime), static_cast<time_t>(inodeBuf.i_mtime),
                static_cast<time_t>(inodeBuf.i_ctime)};
            scope->volume->m_nodes[dirent->inode] = childInode;
            
            RtNode* ctx = scope->rtnode;
            auto child = std::make_shared<RtNode>(
                isDir, size, dirent->inode, inodeBuf.i_mode, inodeBuf.i_uid, inodeBuf.i_gid,
                static_cast<time_t>(inodeBuf.i_atime), static_cast<time_t>(inodeBuf.i_mtime),
                static_cast<time_t>(inodeBuf.i_ctime));
            ctx->children[name] = child;
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

uint32_t Ext4Volume::resolveParentInode(std::string_view path) const {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        return kRootInode;
    }
    
    const std::string parent = normalized.substr(0, normalized.find_last_of('/'));
    if (parent.empty() || parent == "/") {
        return kRootInode;
    }
    
    Inode parentNode;
    if (!resolveNode(parent, &parentNode) || !parentNode.isDirectory) {
        throw IOException("resolveParentInode", std::string(path), "Parent directory does not exist");
    }
    
    return parentNode.ino;
}

std::string Ext4Volume::getBaseName(std::string_view path) const {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        return "";
    }
    
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return normalized;
    }
    
    return normalized.substr(lastSlash + 1);
}

void Ext4Volume::invalidateCache(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    
    // Remove from path cache
    auto fit = m_files.find(normalized);
    if (fit != m_files.end()) {
        ino_t ino = fit->second;
        // Remove from inode cache
        m_nodes.erase(ino);
        // Remove from runtime node cache
        m_rtnodes.erase(ino);
        // Remove from content cache
        m_mem.erase(ino);
        m_files.erase(fit);
    }
    
    // Also invalidate parent directory cache
    const std::string parent = normalized.substr(0, normalized.find_last_of('/'));
    if (!parent.empty() && parent != "/") {
        auto pit = m_files.find(parent);
        if (pit != m_files.end()) {
            m_rtnodes.erase(pit->second);
        }
    }
}
