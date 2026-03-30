#include "MemoryZip.hpp"

#include "../FileStatus.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"
#include "../../io/Uint8ArrayInputStream.hpp"

#include <cstring>
#include <map>
#include <sstream>

#include <unicode/unistr.h>

#include <zlib.h>

#include <sys/stat.h>

// ZIP file format structures
#pragma pack(push, 1)
struct ZipLocalFileHeader {
    uint32_t signature; // 0x04034b50
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLen;
    uint16_t extraFieldLen;
};

struct ZipCentralDirEntry {
    uint32_t signature; // 0x02014b50
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t filenameLen;
    uint16_t extraFieldLen;
    uint16_t commentLen;
    uint16_t diskNum;
    uint16_t internalAttrs;
    uint32_t externalAttrs;
    uint32_t localHeaderOffset;
};

struct ZipEndOfCentralDir {
    uint32_t signature; // 0x06054b50
    uint16_t diskNum;
    uint16_t centralDirDisk;
    uint16_t centralDirRecords;
    uint16_t totalCentralDirRecords;
    uint32_t centralDirSize;
    uint32_t centralDirOffset;
    uint16_t commentLen;
};
#pragma pack(pop)

MemoryZip::MemoryZip(const uint8_t* data, size_t length) : m_data(data), m_size(length) {
    parseZip();
}

template <typename T> std::string toHex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

std::string MemoryZip::getId() const {
    // offset:length
    std::string offsetHex = toHex(reinterpret_cast<uintptr_t>(m_data));
    std::string lengthHex = toHex(m_size);
    return offsetHex + ":" + lengthHex;
}

std::string MemoryZip::getDefaultLabel() const { return "Memory Zip"; }

bool MemoryZip::exists(std::string_view path) const {
    std::string pathStr(path);
    return findEntry(pathStr) != nullptr;
}

bool MemoryZip::isFile(std::string_view path) const {
    std::string pathStr(path);
    const ZipEntry* entry = findEntry(pathStr);
    return entry != nullptr && !entry->isDirectory;
}

bool MemoryZip::isDirectory(std::string_view path) const {
    std::string pathStr(path);
    const ZipEntry* entry = findEntry(pathStr);
    if (entry && entry->isDirectory) {
        return true;
    }

    // Also check if there are any entries under this path
    std::string normalizedPath = pathStr;
    if (!normalizedPath.empty() && normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }
    if (!normalizedPath.empty() && normalizedPath.back() != '/') {
        normalizedPath += "/";
    }

    for (const auto& pair : m_entries) {
        if (pair.first.find(normalizedPath) == 0 && pair.first != normalizedPath) {
            return true;
        }
    }

    return false;
}

void MemoryZip::readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list,
                                std::string_view path, bool recursive) {
    std::string pathStr(path);

    // Normalize path
    if (pathStr.empty() || pathStr == "/") {
        pathStr = "";
    } else {
        if (pathStr[0] != '/') {
            pathStr = "/" + pathStr;
        }
        if (pathStr.back() != '/') {
            pathStr += "/";
        }
    }

    // Find all entries under this path
    std::map<std::string, bool> seen; // Track seen entries to avoid duplicates

    for (const auto& pair : m_entries) {
        const std::string& entryName = pair.first;

        auto st = std::make_unique<FileStatus>();
        // Skip if not under this path
        if (pathStr.empty()) {
            // Root directory - only show top-level entries
            size_t firstSlash = entryName.find('/', 1);
            if (firstSlash == std::string::npos) {
                // Top-level entry (no subdirectories)
                std::string name = entryName.substr(1); // Remove leading /
                if (name.back() == '/') {
                    name.pop_back();
                }
                if (seen.find(name) != seen.end()) {
                    continue;
                }
                seen[name] = true;

                st->name = name;
                st->type = pair.second.isDirectory ? DIRECTORY : REGULAR_FILE;
                st->size = pair.second.uncompressedSize;
                st->modifiedTime = 0;
            } else {
                // Extract first component
                std::string topLevel = entryName.substr(1, firstSlash - 1);
                if (seen.find(topLevel) != seen.end()) {
                    continue;
                }
                seen[topLevel] = true;

                st->name = topLevel;
                st->type = DIRECTORY;
                st->size = 0;
                st->modifiedTime = 0;
            }
        } else {
            if (entryName.find(pathStr) != 0) {
                continue;
            }

            // Extract relative name
            std::string relative = entryName.substr(pathStr.length());
            if (relative.empty()) {
                continue;
            }

            // Remove leading slash if present
            if (relative[0] == '/') {
                relative = relative.substr(1);
            }

            // Check if it's a direct child (not nested)
            size_t nextSlash = relative.find('/');
            if (nextSlash != std::string::npos) {
                // It's nested - extract the directory name
                std::string dirName = relative.substr(0, nextSlash);
                std::string fullDirName = pathStr + dirName + "/";
                if (seen.find(fullDirName) != seen.end()) {
                    continue;
                }
                seen[fullDirName] = true;

                st->name = dirName;
                st->type = DIRECTORY;
                st->size = 0;
                st->modifiedTime = 0;
            } else {
                // Direct child
                if (seen.find(relative) != seen.end()) {
                    continue;
                }
                seen[relative] = true;

                st->name = relative;
                st->type = pair.second.isDirectory ? DIRECTORY : REGULAR_FILE;
                st->size = pair.second.uncompressedSize;
                st->modifiedTime = 0;
            }
        } // if pathStr.empty()

        st->mode = S_IRUSR | S_IRGRP | S_IROTH;
        if (st->type == DIRECTORY)
            st->mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
        else
            st->mode |= S_IFREG;

        list.push_back(std::move(st));
    } // for m_entries
}

bool MemoryZip::stat(std::string_view path, FileStatus* status) const {
    std::string pathStr(path);
    const ZipEntry* entry = findEntry(pathStr);

    if (entry) {
        *status = FileStatus();
        status->name = pathStr;
        status->type = entry->isDirectory ? DIRECTORY : REGULAR_FILE;
        status->size = entry->uncompressedSize;
        status->modifiedTime = 0; // ZIP doesn't preserve timestamps in our implementation
        status->accessTime = 0;
        status->createTime = 0;
        return true;
    }

    // Check if it's a directory by looking for entries under this path
    std::string normalizedPath = pathStr;
    if (!normalizedPath.empty() && normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }
    if (!normalizedPath.empty() && normalizedPath.back() != '/') {
        normalizedPath += "/";
    }

    bool found = false;
    for (const auto& pair : m_entries) {
        if (pair.first.find(normalizedPath) == 0 && pair.first != normalizedPath) {
            found = true;
            break;
        }
    }

    if (found) {
        *status = FileStatus();
        status->name = pathStr;
        status->type = DIRECTORY;
        status->size = 0;
        status->modifiedTime = 0;
        status->accessTime = 0;
        status->createTime = 0;
        return true;
    }

    return false;

    return status;
}

// std::unique_ptr<IReadStream> MemoryZip::openForRead(std::string_view path, std::string_view
// encoding) {
//     std::vector<uint8_t> data = readFile(path);
//     return
//     std::make_unique<DecodingReadStream>(std::make_unique<Uint8ArrayInputStream>(std::move(data)),
//     encoding);
// }

// std::unique_ptr<IWriteStream> MemoryZip::openForWrite(std::string_view path, bool append,
// std::string_view /*encoding*/) {
//     (void)path;
//     (void)append;
//     throw IOException("openForWrite", std::string(path), "MemoryZip is read-only");
// }

std::unique_ptr<InputStream> MemoryZip::newInputStream(std::string_view path) {
    std::vector<uint8_t> data = readFile(path);
    return std::make_unique<Uint8ArrayInputStream>(std::move(data));
}

std::unique_ptr<RandomInputStream> MemoryZip::newRandomInputStream(std::string_view path) {
    std::vector<uint8_t> data = readFile(path);
    return std::make_unique<Uint8ArrayInputStream>(std::move(data));
}

std::unique_ptr<RandomReader> MemoryZip::newRandomReader(std::string_view path,
                                                         std::string_view encoding) {
    return Volume::newRandomReader(path, encoding);
}

std::unique_ptr<OutputStream> MemoryZip::newOutputStream(std::string_view path, bool append) {
    throw IOException("newOutputStream", std::string(path), "MemoryZip is read-only");
}

std::unique_ptr<Reader> MemoryZip::newReader(std::string_view path, std::string_view encoding) {
    std::vector<uint8_t> data = readFile(path);
    // icu decode the data
    icu::UnicodeString unicodeString = icu::UnicodeString::fromUTF8(
        icu::StringPiece(reinterpret_cast<const char*>(data.data()), data.size()));
    std::string text;
    unicodeString.toUTF8String(text);
    return std::make_unique<StringReader>(text);
}

std::unique_ptr<Writer> MemoryZip::newWriter(std::string_view path, bool append,
                                             std::string_view encoding) {
    throw IOException("newWriter", std::string(path), "MemoryZip is read-only");
}

std::vector<uint8_t> MemoryZip::readFileUnchecked(std::string_view path) {
    std::string pathStr(path);
    const ZipEntry* entry = findEntry(pathStr);
    if (!entry || entry->isDirectory) {
        throw IOException("readFile", pathStr, "File not found or is directory");
    }
    return decompressEntry(*entry);
}

void MemoryZip::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    (void)path; // Suppress unused parameter warning
    (void)data;
    throw IOException("writeFile", std::string(path), "MemoryZip is read-only");
}

void MemoryZip::createDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("createDirectory", std::string(path), "MemoryZip is read-only");
}

void MemoryZip::removeDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("removeDirectory", std::string(path), "MemoryZip is read-only");
}

void MemoryZip::removeFileThrowsUnchecked(std::string_view path) {
    throw IOException("removeFile", std::string(path), "MemoryZip is read-only");
}

void MemoryZip::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    (void)src; // Suppress unused parameter warning
    (void)dest;
    throw IOException("copyFile", std::string(src), "MemoryZip is read-only");
}

void MemoryZip::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    (void)src; // Suppress unused parameter warning
    (void)dest;
    throw IOException("moveFile", std::string(src), "MemoryZip is read-only");
}

void MemoryZip::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) {
    (void)oldPath; // Suppress unused parameter warning
    (void)newPath;
    throw IOException("rename", std::string(oldPath), "MemoryZip is read-only");
}

std::string MemoryZip::getLocalFile(std::string_view path) const {
    (void)path; // Suppress unused parameter warning
    return "";  // MemoryZip doesn't map to local files
}

std::string MemoryZip::getTempDir() {
    throw IOException("getTempDir", "", "MemoryZip is read-only");
}

std::string MemoryZip::createTempFile(std::string_view prefix, std::string_view suffix) {
    (void)prefix; // Suppress unused parameter warning
    (void)suffix;
    throw IOException("createTempFile", "", "MemoryZip is read-only");
}

void MemoryZip::parseZip() {
    m_entries.clear();

    if (!m_data || m_size < sizeof(ZipEndOfCentralDir)) {
        return;
    }

    // Find End of Central Directory Record (starts from the end)
    const ZipEndOfCentralDir* eocd = nullptr;
    size_t searchStart = m_size - sizeof(ZipEndOfCentralDir);
    if (searchStart > 65536) {
        searchStart = m_size - 65536; // EOCD is usually within last 64KB
    }

    for (size_t i = searchStart; i < m_size; ++i) {
        if (i + sizeof(ZipEndOfCentralDir) > m_size) {
            break;
        }
        const ZipEndOfCentralDir* candidate =
            reinterpret_cast<const ZipEndOfCentralDir*>(m_data + i);
        if (candidate->signature == 0x06054b50) {
            eocd = candidate;
            break;
        }
    }

    if (!eocd) {
        return; // Not a valid ZIP file
    }

    // Read central directory entries
    size_t cdOffset = eocd->centralDirOffset;
    if (cdOffset + eocd->centralDirSize > m_size) {
        return;
    }

    for (uint16_t i = 0; i < eocd->totalCentralDirRecords; ++i) {
        if (cdOffset + sizeof(ZipCentralDirEntry) > m_size) {
            break;
        }

        const ZipCentralDirEntry* cde =
            reinterpret_cast<const ZipCentralDirEntry*>(m_data + cdOffset);
        if (cde->signature != 0x02014b50) {
            break;
        }

        size_t filenameOffset = cdOffset + sizeof(ZipCentralDirEntry);
        if (filenameOffset + cde->filenameLen + cde->extraFieldLen + cde->commentLen > m_size) {
            break;
        }

        // Read filename
        std::string filename(reinterpret_cast<const char*>(m_data + filenameOffset),
                             cde->filenameLen);

        ZipEntry entry;
        entry.name = filename;
        entry.localHeaderOffset = cde->localHeaderOffset;
        entry.compressedSize = cde->compressedSize;
        entry.uncompressedSize = cde->uncompressedSize;
        entry.compressionMethod = cde->compression;

        // Check if it's a directory (ZIP convention: ends with /)
        entry.isDirectory = (!filename.empty() && filename.back() == '/');

        // Normalize path (ensure it starts with /)
        std::string normalizedName = filename;
        if (!normalizedName.empty() && normalizedName[0] != '/') {
            normalizedName = "/" + normalizedName;
        }

        m_entries[normalizedName] = entry;

        cdOffset +=
            sizeof(ZipCentralDirEntry) + cde->filenameLen + cde->extraFieldLen + cde->commentLen;
    }
}

const MemoryZip::ZipEntry* MemoryZip::findEntry(const std::string& path) const {
    std::string normalizedPath = path;
    if (!normalizedPath.empty() && normalizedPath[0] != '/') {
        normalizedPath = "/" + normalizedPath;
    }

    auto it = m_entries.find(normalizedPath);
    if (it != m_entries.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint8_t> MemoryZip::decompressEntry(const ZipEntry& entry) const {
    if (entry.localHeaderOffset + sizeof(ZipLocalFileHeader) > m_size) {
        throw IOException("decompressEntry", entry.name, "Invalid ZIP entry offset");
    }

    const ZipLocalFileHeader* lfh =
        reinterpret_cast<const ZipLocalFileHeader*>(m_data + entry.localHeaderOffset);
    if (lfh->signature != 0x04034b50) {
        throw IOException("decompressEntry", entry.name, "Invalid local file header");
    }

    size_t dataOffset = entry.localHeaderOffset + sizeof(ZipLocalFileHeader) + lfh->filenameLen +
                        lfh->extraFieldLen;
    if (dataOffset + entry.compressedSize > m_size) {
        throw IOException("decompressEntry", entry.name, "Invalid ZIP entry size");
    }

    if (entry.compressionMethod == 0) {
        // Stored (no compression) - just copy the data
        std::vector<uint8_t> data(entry.uncompressedSize);
        memcpy(data.data(), m_data + dataOffset, entry.uncompressedSize);
        return data;
    } else if (entry.compressionMethod == 8) {
        // Deflate compression - use zlib
        z_stream zs;
        memset(&zs, 0, sizeof(zs));

        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
            throw IOException("decompressEntry", entry.name, "Failed to initialize zlib");
        }

        zs.next_in = const_cast<uint8_t*>(m_data + dataOffset);
        zs.avail_in = entry.compressedSize;

        std::vector<uint8_t> data(entry.uncompressedSize);
        zs.next_out = data.data();
        zs.avail_out = entry.uncompressedSize;

        int ret = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);

        if (ret != Z_STREAM_END) {
            throw IOException("decompressEntry", entry.name, "Failed to decompress");
        }

        return data;
    } else {
        throw IOException("decompressEntry", entry.name, "Unsupported compression method");
    }
}
