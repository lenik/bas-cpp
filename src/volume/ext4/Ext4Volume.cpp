#include "Ext4Volume.hpp"

#include "Ext4FileInputStream.hpp"
#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"

#include <functional>
#include <stdexcept>
#include <unordered_set>
#include <vector>
#include <sys/stat.h>

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#endif

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

std::string Ext4Volume::getDefaultLabel() const {
    return "EXT Image";
}

std::string Ext4Volume::getLocalFile(std::string_view /*path*/) const {
    return "";
}

std::string Ext4Volume::normalizePath(std::string_view path) const {
    return Volume::normalize(path, true);
}

bool Ext4Volume::exists(std::string_view path) const {
    Node node;
    return resolveNode(path, &node);
}

bool Ext4Volume::isFile(std::string_view path) const {
    Node node;
    return resolveNode(path, &node) && !node.isDirectory;
}

bool Ext4Volume::isDirectory(std::string_view path) const {
    Node node;
    return resolveNode(path, &node) && node.isDirectory;
}

bool Ext4Volume::stat(std::string_view path, FileStatus* status) const {
    if (!status) {
        throw std::invalid_argument("Ext4Volume::stat: status is null");
    }
    Node node;
    const std::string normalized = normalizePath(path);
    if (!resolveNode(normalized, &node)) {
        return false;
    }
    status->name = normalized;
    status->type = node.isDirectory ? DIRECTORY : REGULAR_FILE;
    status->size = node.size;
    status->mode = node.mode;
    status->uid = node.uid;
    status->gid = node.gid;
    status->modifiedTime = 0;
    status->accessTime = 0;
    status->createTime = 0;
    return true;
}

void Ext4Volume::readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path,
                                 bool recursive) {
    const std::string parent = normalizePath(path);
    Node parentNode;
    if (!resolveNode(parent, &parentNode) || !parentNode.isDirectory) {
        throw IOException("readDir", std::string(path), "Path is not a directory");
    }
    if (!checkMode(parentNode, 4 | 1)) { // r+x
        throw IOException("readDir", std::string(path), "Permission denied");
    }

    const auto& children = getDirectoryEntries(parentNode.inode);
    if (!recursive) {
        for (const auto& [name, node] : children) {
            auto st = std::make_unique<FileStatus>();
            st->name = name;
            st->type = node.isDirectory ? DIRECTORY : REGULAR_FILE;
            st->size = node.size;
            list.push_back(std::move(st));
        }
        return;
    }

    std::unordered_set<uint32_t> visitedDirs;
    std::function<void(const std::string&, const Node&)> walk = [&](const std::string& basePath,
                                                                    const Node& dirNode) {
        if (!visitedDirs.insert(dirNode.inode).second) {
            return;
        }
        const auto& ents = getDirectoryEntries(dirNode.inode);
        for (const auto& [name, child] : ents) {
            std::string absPath = (basePath == "/") ? ("/" + name) : (basePath + "/" + name);
            std::string relPath = (parent == "/") ? absPath.substr(1) : absPath.substr(parent.size() + 1);

            auto st = std::make_unique<FileStatus>();
            st->name = relPath;
            st->type = child.isDirectory ? DIRECTORY : REGULAR_FILE;
            st->size = child.size;
            list.push_back(std::move(st));

            m_nodes[absPath] = child;
            if (child.isDirectory) {
                walk(absPath, child);
            }
        }
    };
    walk(parent, parentNode);
}

std::vector<uint8_t> Ext4Volume::readFile(std::string_view path) {
    const std::string normalized = normalizePath(path);
    Node node;
    if (!resolveNode(normalized, &node) || node.isDirectory) {
        throw IOException("readFile", std::string(path), "File not found or is a directory");
    }
    if (!checkMode(node, 4)) {
        throw IOException("readFile", std::string(path), "Permission denied");
    }

    auto in = newInputStream(path);
    if (!in) return {};
    auto data = in->readBytesUntilEOF();
    if (data.size() > node.size) data.resize(static_cast<size_t>(node.size));
    return data;
}

std::unique_ptr<InputStream> Ext4Volume::newInputStream(std::string_view path) {
    const std::string normalized = normalizePath(path);
    Node node;
    if (!resolveNode(normalized, &node) || node.isDirectory) {
        throw IOException("newInputStream", std::string(path), "File not found or is a directory");
    }
    if (!checkMode(node, 4)) {
        throw IOException("newInputStream", std::string(path), "Permission denied");
    }
    return std::make_unique<Ext4FileInputStream>(m_imagePath, node.inode);
}

std::unique_ptr<RandomInputStream> Ext4Volume::newRandomInputStream(std::string_view path) {
    auto in = newInputStream(path);
    return std::unique_ptr<RandomInputStream>(dynamic_cast<RandomInputStream*>(in.release()));
}

std::unique_ptr<Reader> Ext4Volume::newReader(std::string_view path, std::string_view /*encoding*/) {
    auto bytes = readFile(path);
    return std::make_unique<StringReader>(std::string(bytes.begin(), bytes.end()));
}

std::unique_ptr<RandomReader> Ext4Volume::newRandomReader(std::string_view path, std::string_view encoding) {
    return Volume::newRandomReader(path, encoding);
}

bool Ext4Volume::createDirectory(std::string_view path) {
    throw IOException("createDirectory", std::string(path), "Ext4Volume write operations are not implemented yet");
}

bool Ext4Volume::removeDirectory(std::string_view path) {
    throw IOException("removeDirectory", std::string(path), "Ext4Volume write operations are not implemented yet");
}

std::unique_ptr<OutputStream> Ext4Volume::newOutputStream(std::string_view path, bool append) {
    return std::make_unique<Ext4FileOutputStream>(m_imagePath, std::string(path), append);
}

std::unique_ptr<Writer> Ext4Volume::newWriter(std::string_view path, bool /*append*/,
                                              std::string_view /*encoding*/) {
    throw IOException("newWriter", std::string(path), "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::writeFile(std::string_view path, const std::vector<uint8_t>& /*data*/) {
    throw IOException("writeFile", std::string(path), "Ext4Volume write operations are not implemented yet");
}

std::string Ext4Volume::getTempDir() {
    throw IOException("getTempDir", "", "Ext4Volume does not support temp file operations");
}

std::string Ext4Volume::createTempFile(std::string_view /*prefix*/, std::string_view /*suffix*/) {
    throw IOException("createTempFile", "", "Ext4Volume does not support temp file operations");
}

void Ext4Volume::removeFileUnchecked(std::string_view path) {
    throw IOException("removeFile", std::string(path), "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::copyFileUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("copyFile", std::string(src), "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::moveFileUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("moveFile", std::string(src), "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::renameFileUnchecked(std::string_view oldPath, std::string_view /*newPath*/) {
    throw IOException("renameFile", std::string(oldPath), "Ext4Volume write operations are not implemented yet");
}

void Ext4Volume::buildIndex() {
    // Rebuild lightweight caches only. Full tree scan is intentionally avoided.
    m_nodes.clear();
    m_dirIndexCache.clear();
    m_nodes["/"] = Node{true, 0, kRootInode, static_cast<uint16_t>(S_IFDIR | 0755), 0, 0};

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
    // Probe open/close once so mount-time failures are immediate.
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }
    ext2fs_close(fs);
#else
    throw IOException("Ext4Volume", m_imagePath,
                      "Ext4Volume support requires libext2fs development headers/libraries");
#endif
}

bool Ext4Volume::resolveNode(std::string_view path, Node* out) const {
    const std::string normalized = normalizePath(path);
    auto cached = m_nodes.find(normalized);
    if (cached != m_nodes.end()) {
        if (out) *out = cached->second;
        return true;
    }
    if (normalized == "/") {
        if (out) *out = m_nodes.at("/");
        return true;
    }

    Node cur = m_nodes.at("/");
    std::string curPath = "/";

    size_t start = 1;
    while (start <= normalized.size()) {
        size_t slash = normalized.find('/', start);
        std::string part = normalized.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (part.empty()) break;

        if (!checkMode(cur, 1)) { // execute/search on current directory
            return false;
        }
        const auto& ents = getDirectoryEntries(cur.inode);
        auto it = ents.find(part);
        if (it == ents.end()) {
            return false;
        }
        cur = it->second;
        curPath = (curPath == "/") ? ("/" + part) : (curPath + "/" + part);
        m_nodes[curPath] = cur;

        if (slash == std::string::npos) break;
        start = slash + 1;
    }

    if (curPath != normalized) {
        return false;
    }
    if (out) *out = cur;
    return true;
}

const std::unordered_map<std::string, Ext4Volume::Node>& Ext4Volume::getDirectoryEntries(uint32_t inode) const {
    auto itCache = m_dirIndexCache.find(inode);
    if (itCache != m_dirIndexCache.end()) {
        return itCache->second;
    }

    std::unordered_map<std::string, Node> entries;

#if defined(BAS_HAS_EXT2FS) && BAS_HAS_EXT2FS
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS, 0, 0, unix_io_manager, &fs);
    if (rc) {
        throw IOException("Ext4Volume", m_imagePath, "ext2fs_open failed");
    }

    struct DirCtx {
        ext2_filsys fs;
        std::unordered_map<std::string, Node>* entries;
    } ctx{fs, &entries};

    ext2fs_dir_iterate2(
        fs, static_cast<ext2_ino_t>(inode), 0, nullptr,
        [](ext2_ino_t, int, struct ext2_dir_entry* dirent, int, int, char*, void* priv) -> int {
            auto* c = static_cast<DirCtx*>(priv);
            if (!dirent || dirent->inode == 0) return 0;
            std::string name(dirent->name, dirent->name + dirent->name_len);
            if (name == "." || name == "..") return 0;

            struct ext2_inode inodeBuf {};
            if (ext2fs_read_inode(c->fs, dirent->inode, &inodeBuf) != 0) return 0;
            const bool isDir = LINUX_S_ISDIR(inodeBuf.i_mode);
            const uint64_t size = ext2fs_file_inode_size(c->fs, &inodeBuf);
            (*c->entries)[name] = Node{isDir, size, dirent->inode, inodeBuf.i_mode, inodeBuf.i_uid,
                                       inodeBuf.i_gid};
            return 0;
        },
        &ctx);

    ext2fs_close(fs);
#else
    throw IOException("Ext4Volume", m_imagePath,
                      "Ext4Volume support requires libext2fs development headers/libraries");
#endif

    auto inserted = m_dirIndexCache.emplace(inode, std::move(entries));
    return inserted.first->second;
}

bool Ext4Volume::hasGroup(uint32_t gid) const {
    for (int g : m_contextGroupIds) {
        if (g == static_cast<int>(gid)) {
            return true;
        }
    }
    return false;
}

bool Ext4Volume::checkMode(const Node& node, int needMask) const {
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
