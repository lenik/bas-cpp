#include "MemoryZip.hpp"

#include "../FileStatus.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"
#include "../../io/Uint8ArrayInputStream.hpp"

#include <cstring>
#include <ctime>
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

namespace {

uint16_t readLE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

/** ZIP DOS time/date (central / local header) to Unix time (local timezone). */
time_t dosDateTimeToTimeT(uint16_t dosTime, uint16_t dosDate) {
    if (dosTime == 0 && dosDate == 0)
        return 0;
    int sec = (dosTime & 0x1F) * 2;
    int min = (dosTime >> 5) & 0x3F;
    int hour = (dosTime >> 11) & 0x1F;
    int day = dosDate & 0x1F;
    int month = (dosDate >> 5) & 0x0F;
    int year = (dosDate >> 9) + 1980;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || min > 59 || sec > 59)
        return 0;
    struct tm tm = {};
    tm.tm_sec = sec;
    tm.tm_min = min;
    tm.tm_hour = hour;
    tm.tm_mday = day;
    tm.tm_mon = month - 1;
    tm.tm_year = year - 1900;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

/** Little-endian integer; nbytes in 1..8 (Info-ZIP variable-width UID/GID). */
uint32_t readLEVarU32(const uint8_t* p, size_t nbytes) {
    if (nbytes == 0 || nbytes > 8)
        return 0;
    uint64_t v = 0;
    for (size_t i = 0; i < nbytes; ++i)
        v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return static_cast<uint32_t>(v & 0xffffffffu);
}

struct ZipExtraParsed {
    time_t utMtime = 0;
    time_t utAtime = 0;
    time_t utCtime = 0;
    bool hasUtMtime = false;
    bool hasUtAtime = false;
    bool hasUtCtime = false;
    bool hasUx = false;
    unsigned uid = 0;
    unsigned gid = 0;
};

/** Central-directory extra: UT (0x5455) mtime/atime/ctime, New Unix (0x7875) uid/gid. */
void scanZipCentralExtraField(const uint8_t* data, uint16_t len, ZipExtraParsed* out) {
    out->hasUtMtime = out->hasUtAtime = out->hasUtCtime = false;
    out->hasUx = false;
    size_t pos = 0;
    while (pos + 4 <= len) {
        uint16_t headerId = readLE16(data + pos);
        uint16_t dataSize = readLE16(data + pos + 2);
        pos += 4;
        if (pos + dataSize > len)
            break;
        const uint8_t* payload = data + pos;

        if (headerId == 0x5455 && dataSize >= 1) {
            uint8_t flags = payload[0];
            size_t off = 1;
            if ((flags & 1) != 0 && off + 4 <= dataSize) {
                time_t t = static_cast<time_t>(readLE32(payload + off));
                if (t > 0) {
                    out->utMtime = t;
                    out->hasUtMtime = true;
                }
                off += 4;
            }
            if ((flags & 2) != 0 && off + 4 <= dataSize) {
                time_t t = static_cast<time_t>(readLE32(payload + off));
                if (t > 0) {
                    out->utAtime = t;
                    out->hasUtAtime = true;
                }
                off += 4;
            }
            if ((flags & 4) != 0 && off + 4 <= dataSize) {
                time_t t = static_cast<time_t>(readLE32(payload + off));
                if (t > 0) {
                    out->utCtime = t;
                    out->hasUtCtime = true;
                }
            }
        } else if (headerId == 0x7875 && dataSize >= 3) {
            uint8_t uidSize = payload[1];
            if (2 + uidSize <= dataSize) {
                size_t gidSizeIdx = 2 + uidSize;
                if (gidSizeIdx < dataSize) {
                    uint8_t gidSize = payload[gidSizeIdx];
                    if (gidSizeIdx + 1 + gidSize <= dataSize) {
                        out->hasUx = true;
                        out->uid = readLEVarU32(payload + 2, uidSize);
                        out->gid = readLEVarU32(payload + gidSizeIdx + 1, gidSize);
                    }
                }
            }
        }
        pos += dataSize;
    }
}

} // namespace

MemoryZip::MemoryZip(std::string_view sym, const uint8_t* data, size_t length)
    : m_sym(sym), m_data(data), m_size(length) {
    parseZip();
}

template <typename T> std::string toHex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

std::string MemoryZip::getClass() const { return "mz"; }

std::string MemoryZip::getId() const {
    // offset:length
    std::string offsetHex = toHex(reinterpret_cast<uintptr_t>(m_data));
    std::string lengthHex = toHex(m_size);
    return offsetHex + ":" + lengthHex;
}

VolumeType MemoryZip::getType() const { return VolumeType::ARCHIVE; }

std::string MemoryZip::getSource() const {
    // mem sym:offset:length
    return std::string("mz ") + m_sym                        //
           + ":" + toHex(reinterpret_cast<uintptr_t>(m_data)) //
           + ":" + toHex(m_size);
}

std::string MemoryZip::getDefaultLabel() const { return "Memory Zip"; }

bool MemoryZip::exists(std::string_view path) const {
    std::string pathStr(path);
    if (pathStr.empty() || pathStr == "/") return true;
    if (findEntry(pathStr) != nullptr)
        return true;
    if (!pathStr.empty()) {
        if (pathStr.back() != '/') {
            if (findEntry(pathStr + "/") != nullptr)
                return true;
        }
    }
    return false;
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
                st->modifiedTime = pair.second.modifiedTime;
                st->accessTime =
                    pair.second.accessTime != 0 ? pair.second.accessTime : pair.second.modifiedTime;
                st->creationTime = pair.second.creationTime != 0 ? pair.second.creationTime
                                                                 : pair.second.modifiedTime;
                if (pair.second.hasUnixIds) {
                    st->uid = pair.second.uid;
                    st->gid = pair.second.gid;
                }
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
                st->modifiedTime = pair.second.modifiedTime;
                st->accessTime =
                    pair.second.accessTime != 0 ? pair.second.accessTime : pair.second.modifiedTime;
                st->creationTime = pair.second.creationTime != 0 ? pair.second.creationTime
                                                                 : pair.second.modifiedTime;
                if (pair.second.hasUnixIds) {
                    st->uid = pair.second.uid;
                    st->gid = pair.second.gid;
                }
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
        status->modifiedTime = entry->modifiedTime;
        status->accessTime =
            entry->accessTime != 0 ? entry->accessTime : entry->modifiedTime;
        status->creationTime =
            entry->creationTime != 0 ? entry->creationTime : entry->modifiedTime;
        if (entry->hasUnixIds) {
            status->uid = entry->uid;
            status->gid = entry->gid;
        }
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
        status->creationTime = 0;
        return true;
    }

    return false;
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

        const uint8_t* extraPtr = m_data + filenameOffset + cde->filenameLen;
        uint16_t extraLen = cde->extraFieldLen;

        ZipEntry entry;
        entry.name = filename;
        entry.localHeaderOffset = cde->localHeaderOffset;
        entry.compressedSize = cde->compressedSize;
        entry.uncompressedSize = cde->uncompressedSize;
        entry.compressionMethod = cde->compression;

        ZipExtraParsed extra{};
        if (extraLen > 0 && filenameOffset + cde->filenameLen + extraLen <= m_size)
            scanZipCentralExtraField(extraPtr, extraLen, &extra);
        entry.modifiedTime =
            extra.hasUtMtime ? extra.utMtime : dosDateTimeToTimeT(cde->modTime, cde->modDate);
        entry.accessTime = extra.hasUtAtime ? extra.utAtime : 0;
        entry.creationTime = extra.hasUtCtime ? extra.utCtime : 0;
        if (extra.hasUx) {
            entry.hasUnixIds = true;
            entry.uid = extra.uid;
            entry.gid = extra.gid;
        }

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
