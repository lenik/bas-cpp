#include "Fat32Volume.hpp"

#include "Fat32FileInputStream.hpp"
#include "Fat32FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace {

uint16_t le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
           | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

std::string trimRight(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && s[end - 1] == ' ')
        --end;
    return s.substr(0, end);
}

bool isDotEntry(std::string_view name) {
    return name == "." || name == "..";
}

} // namespace

Fat32Volume::Fat32Volume(std::string_view imagePath) : m_imagePath(imagePath) {
    if (m_imagePath.empty()) {
        throw std::invalid_argument("Fat32Volume: image path is required");
    }

    std::ifstream in(m_imagePath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        throw IOException("Fat32Volume", m_imagePath, "Failed to open image file");
    }
    const std::streamoff size = in.tellg();
    if (size <= 0) {
        throw IOException("Fat32Volume", m_imagePath, "Image file is empty");
    }
    m_imageSize = static_cast<uint64_t>(size);

    parseBootSector();
    buildIndex();
}

std::string Fat32Volume::getDefaultLabel() const {
    return "FAT32 Image";
}

std::string Fat32Volume::getLocalFile(std::string_view /*path*/) const {
    return "";
}

std::string Fat32Volume::normalizePath(std::string_view path) const {
    return Volume::normalize(path, true);
}

bool Fat32Volume::exists(std::string_view path) const {
    return m_nodes.find(normalizePath(path)) != m_nodes.end();
}

bool Fat32Volume::isFile(std::string_view path) const {
    auto it = m_nodes.find(normalizePath(path));
    return it != m_nodes.end() && !it->second.isDirectory;
}

bool Fat32Volume::isDirectory(std::string_view path) const {
    auto it = m_nodes.find(normalizePath(path));
    return it != m_nodes.end() && it->second.isDirectory;
}

bool Fat32Volume::stat(std::string_view path, FileStatus* status) const {
    if (!status) {
        throw std::invalid_argument("Fat32Volume::stat: status is null");
    }
    auto it = m_nodes.find(normalizePath(path));
    if (it == m_nodes.end()) {
        return false;
    }
    status->name = normalizePath(path);
    status->type = it->second.isDirectory ? DIRECTORY : REGULAR_FILE;
    status->size = it->second.size;
    status->modifiedTime = 0;
    status->accessTime = 0;
    status->createTime = 0;
    return true;
}

void Fat32Volume::readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path,
                                  bool recursive) {
    const std::string parent = normalizePath(path);
    auto pit = m_nodes.find(parent);
    if (pit == m_nodes.end() || !pit->second.isDirectory) {
        throw IOException("readDir", std::string(path), "Path is not a directory");
    }

    const std::string prefix = (parent == "/") ? "/" : parent + "/";

    for (const auto& [fullPath, node] : m_nodes) {
        if (fullPath == parent || fullPath.rfind(prefix, 0) != 0) {
            continue;
        }

        const std::string rel = fullPath.substr(prefix.size());
        if (rel.empty()) {
            continue;
        }

        if (!recursive && rel.find('/') != std::string::npos) {
            continue;
        }

        auto st = std::make_unique<FileStatus>();
        st->name = recursive ? rel : rel.substr(0, rel.find('/'));
        st->type = node.isDirectory ? DIRECTORY : REGULAR_FILE;
        st->size = node.size;
        st->modifiedTime = 0;
        st->accessTime = 0;
        st->createTime = 0;
        list.push_back(std::move(st));
    }

    if (!recursive) {
        std::unordered_set<std::string> seen;
        std::vector<std::unique_ptr<FileStatus>> unique;
        unique.reserve(list.size());
        for (auto& item : list) {
            if (seen.insert(item->name).second) {
                unique.push_back(std::move(item));
            }
        }
        list = std::move(unique);
    }
}

std::vector<uint8_t> Fat32Volume::readFile(std::string_view path) {
    const std::string normalized = normalizePath(path);
    auto it = m_nodes.find(normalized);
    if (it == m_nodes.end() || it->second.isDirectory) {
        throw IOException("readFile", std::string(path), "File not found or is a directory");
    }

    const Node& node = it->second;
    std::vector<uint8_t> out;
    out.reserve(node.size);

    if (node.size == 0 || node.firstCluster < 2) {
        return out;
    }

    auto stream = newInputStream(path);
    if (!stream) {
        return {};
    }
    out = stream->readBytesUntilEOF();
    if (out.size() > node.size) out.resize(node.size);
    return out;
}

std::unique_ptr<InputStream> Fat32Volume::newInputStream(std::string_view path) {
    const std::string normalized = normalizePath(path);
    auto it = m_nodes.find(normalized);
    if (it == m_nodes.end() || it->second.isDirectory) {
        throw IOException("newInputStream", std::string(path), "File not found or is a directory");
    }
    std::vector<uint32_t> chain;
    if (it->second.firstCluster >= 2 && it->second.size > 0) {
        chain = readClusterChain(it->second.firstCluster);
    }
    auto stream = std::make_unique<Fat32FileInputStream>(m_imagePath, std::move(chain), m_dataOffset,
                                                          m_clusterSize, it->second.size);
    if (!stream->isOpen()) {
        throw IOException("newInputStream", normalized, "Failed to open image");
    }
    return stream;
}

std::unique_ptr<RandomInputStream> Fat32Volume::newRandomInputStream(std::string_view path) {
    auto stream = newInputStream(path);
    return std::unique_ptr<RandomInputStream>(dynamic_cast<RandomInputStream*>(stream.release()));
}

std::unique_ptr<Reader> Fat32Volume::newReader(std::string_view path, std::string_view /*encoding*/) {
    std::vector<uint8_t> bytes = readFile(path);
    std::string text(bytes.begin(), bytes.end());
    return std::make_unique<StringReader>(text);
}

std::unique_ptr<RandomReader> Fat32Volume::newRandomReader(std::string_view path,
                                                           std::string_view encoding) {
    return Volume::newRandomReader(path, encoding);
}

bool Fat32Volume::createDirectory(std::string_view path) {
    throw IOException("createDirectory", std::string(path),
                      "Fat32Volume write operations are not implemented yet");
}

bool Fat32Volume::removeDirectory(std::string_view path) {
    throw IOException("removeDirectory", std::string(path),
                      "Fat32Volume write operations are not implemented yet");
}

std::unique_ptr<OutputStream> Fat32Volume::newOutputStream(std::string_view path, bool append) {
    return std::make_unique<Fat32FileOutputStream>(m_imagePath, std::string(path), append);
}

std::unique_ptr<Writer> Fat32Volume::newWriter(std::string_view path, bool /*append*/,
                                               std::string_view /*encoding*/) {
    throw IOException("newWriter", std::string(path), "Fat32Volume write operations are not implemented yet");
}

void Fat32Volume::writeFile(std::string_view path, const std::vector<uint8_t>& /*data*/) {
    throw IOException("writeFile", std::string(path), "Fat32Volume write operations are not implemented yet");
}

void Fat32Volume::removeFileUnchecked(std::string_view path) {
    throw IOException("removeFile", std::string(path), "Fat32Volume write operations are not implemented yet");
}

void Fat32Volume::copyFileUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("copyFile", std::string(src), "Fat32Volume write operations are not implemented yet");
}

void Fat32Volume::moveFileUnchecked(std::string_view src, std::string_view /*dest*/) {
    throw IOException("moveFile", std::string(src), "Fat32Volume write operations are not implemented yet");
}

void Fat32Volume::renameFileUnchecked(std::string_view oldPath, std::string_view /*newPath*/) {
    throw IOException("renameFile", std::string(oldPath), "Fat32Volume write operations are not implemented yet");
}

std::string Fat32Volume::getTempDir() {
    throw IOException("getTempDir", "", "Fat32Volume does not support temp file operations");
}

std::string Fat32Volume::createTempFile(std::string_view /*prefix*/, std::string_view /*suffix*/) {
    throw IOException("createTempFile", "", "Fat32Volume does not support temp file operations");
}

void Fat32Volume::parseBootSector() {
    if (m_imageSize < 512) {
        throw IOException("Fat32Volume", m_imagePath, "Invalid image: too small");
    }
    std::vector<uint8_t> boot(512);
    if (!readAt(0, boot.data(), boot.size())) {
        throw IOException("Fat32Volume", m_imagePath, "Failed to read boot sector");
    }
    const uint8_t* b = boot.data();

    m_bytesPerSector = le16(b + 11);
    m_sectorsPerCluster = b[13];
    m_reservedSectors = le16(b + 14);
    m_numFATs = b[16];
    const uint16_t rootEntryCount = le16(b + 17);
    const uint16_t fat16Sectors = le16(b + 22);
    m_sectorsPerFAT = le32(b + 36);
    m_rootCluster = le32(b + 44);

    if (m_bytesPerSector == 0 || m_sectorsPerCluster == 0 || m_reservedSectors == 0 || m_numFATs == 0
        || m_sectorsPerFAT == 0 || m_rootCluster < 2) {
        throw IOException("Fat32Volume", m_imagePath, "Invalid FAT32 boot parameters");
    }
    if (rootEntryCount != 0 || fat16Sectors != 0) {
        throw IOException("Fat32Volume", m_imagePath, "Not a FAT32 image");
    }

    m_clusterSize = static_cast<uint32_t>(m_bytesPerSector) * m_sectorsPerCluster;
    m_fatOffset = static_cast<uint64_t>(m_reservedSectors) * m_bytesPerSector;
    m_dataOffset = static_cast<uint64_t>(m_reservedSectors + (m_numFATs * m_sectorsPerFAT)) * m_bytesPerSector;

    if (m_dataOffset >= m_imageSize) {
        throw IOException("Fat32Volume", m_imagePath, "Invalid FAT32 offsets");
    }
}

void Fat32Volume::buildIndex() {
    m_nodes.clear();
    m_nodes["/"] = Node{true, m_rootCluster, 0};
    parseDirectoryCluster("/", m_rootCluster);
}

uint64_t Fat32Volume::clusterToOffset(uint32_t cluster) const {
    if (cluster < 2) {
        throw IOException("Fat32Volume", m_imagePath, "Invalid cluster number");
    }
    const uint64_t dataClusterIndex = static_cast<uint64_t>(cluster) - 2;
    return m_dataOffset + (dataClusterIndex * m_clusterSize);
}

uint32_t Fat32Volume::getFatEntry(uint32_t cluster) const {
    const uint64_t off = m_fatOffset + static_cast<uint64_t>(cluster) * 4;
    if (off + 4 > m_imageSize) {
        throw IOException("Fat32Volume", m_imagePath, "FAT entry out of range");
    }
    uint8_t b[4] = {0, 0, 0, 0};
    if (!readAt(off, b, sizeof(b))) {
        throw IOException("Fat32Volume", m_imagePath, "Failed to read FAT entry");
    }
    return le32(b) & 0x0FFFFFFF;
}

std::vector<uint32_t> Fat32Volume::readClusterChain(uint32_t firstCluster) const {
    std::vector<uint32_t> chain;
    std::unordered_set<uint32_t> seen;
    uint32_t c = firstCluster;
    while (c >= 2 && c < 0x0FFFFFF8) {
        if (!seen.insert(c).second) {
            throw IOException("Fat32Volume", m_imagePath, "Detected FAT loop");
        }
        chain.push_back(c);
        c = getFatEntry(c);
    }
    return chain;
}

std::string Fat32Volume::decodeShortName(const uint8_t* entry) const {
    std::string name(reinterpret_cast<const char*>(entry), 8);
    std::string ext(reinterpret_cast<const char*>(entry + 8), 3);
    name = trimRight(name);
    ext = trimRight(ext);
    for (char& c : name)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (!ext.empty()) {
        return name + "." + ext;
    }
    return name;
}

std::string Fat32Volume::decodeLfnChunk(const uint8_t* entry) const {
    std::string out;
    auto appendRange = [&out, entry](int start, int count) {
        for (int i = 0; i < count; ++i) {
            const uint16_t ch = le16(entry + start + i * 2);
            if (ch == 0x0000 || ch == 0xFFFF) {
                break;
            }
            if (ch <= 0x7F) {
                out.push_back(static_cast<char>(ch));
            } else {
                out.push_back('?');
            }
        }
    };
    appendRange(1, 5);
    appendRange(14, 6);
    appendRange(28, 2);
    return out;
}

void Fat32Volume::parseDirectoryCluster(std::string_view dirPath, uint32_t firstCluster) {
    if (firstCluster < 2) {
        return;
    }

    std::vector<std::string> lfnParts;
    for (uint32_t cluster : readClusterChain(firstCluster)) {
        const uint64_t off = clusterToOffset(cluster);
        if (off + m_clusterSize > m_imageSize) {
            throw IOException("Fat32Volume", m_imagePath, "Directory cluster out of range");
        }
        std::vector<uint8_t> block(m_clusterSize);
        if (!readAt(off, block.data(), block.size())) {
            throw IOException("Fat32Volume", m_imagePath, "Failed to read directory cluster");
        }

        const uint8_t* p = block.data();
        for (uint32_t pos = 0; pos + 32 <= m_clusterSize; pos += 32) {
            const uint8_t* ent = p + pos;
            if (ent[0] == 0x00) {
                return;
            }
            if (ent[0] == 0xE5) {
                lfnParts.clear();
                continue;
            }
            const uint8_t attr = ent[11];
            if (attr == 0x0F) {
                lfnParts.push_back(decodeLfnChunk(ent));
                continue;
            }
            if ((attr & 0x08) != 0) {
                lfnParts.clear();
                continue;
            }

            std::string name;
            if (!lfnParts.empty()) {
                for (auto it = lfnParts.rbegin(); it != lfnParts.rend(); ++it) {
                    name += *it;
                }
            } else {
                name = decodeShortName(ent);
            }
            lfnParts.clear();

            if (name.empty() || isDotEntry(name)) {
                continue;
            }

            const uint16_t clHi = le16(ent + 20);
            const uint16_t clLo = le16(ent + 26);
            const uint32_t startCluster = (static_cast<uint32_t>(clHi) << 16) | clLo;
            const uint32_t size = le32(ent + 28);
            const bool isDir = (attr & 0x10) != 0;

            std::string parent(dirPath);
            const std::string path = (parent == "/") ? ("/" + name) : (parent + "/" + name);

            m_nodes[path] = Node{isDir, startCluster, isDir ? 0u : size};
            if (isDir && startCluster >= 2) {
                parseDirectoryCluster(path, startCluster);
            }
        }
    }
}

bool Fat32Volume::readAt(uint64_t offset, uint8_t* dst, size_t len) const {
    if (!dst || len == 0 || offset + len > m_imageSize) {
        return false;
    }
    std::ifstream in(m_imagePath, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in.good()) {
        return false;
    }
    in.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(len));
    return in.good() || in.gcount() == static_cast<std::streamsize>(len);
}

