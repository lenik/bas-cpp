#include "ZipVolume.hpp"

#include "../DirNode.hpp"

#include "../../io/IOException.hpp"
#include "../../io/StringReader.hpp"
#include "../../io/Uint8ArrayInputStream.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <map>
#include <optional>
#include <sstream>

#include <bas/log/uselog.h>

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

ZipVolume::ZipVolume(std::shared_ptr<BlockDevice> device, const ZipOptions& options)
    : m_device(device), m_options(options) {
    parseZip();
}

template <typename T> std::string toHex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

std::string to_file_path(std::string_view _path) {
    std::string path(_path);
    // remove leading slash if present
    while (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }
    return path;
}

std::string to_dir_path(std::string_view _path) {
    std::string path(_path);
    // remove leading slash if present
    while (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }
    // add trailing slash if not present
    if (path.empty() || path.back() != '/') {
        path += "/";
    }
    return path;
}

std::string ZipVolume::getClass() const { return "mz"; }

std::string ZipVolume::getUrl() const { return "zip:" + m_device->name(); }

std::string ZipVolume::getDeviceUrl() const { return m_device->uri(); }

VolumeType ZipVolume::getType() const { return VolumeType::ARCHIVE; }

std::string ZipVolume::getDefaultLabel() const { return "Memory Zip"; }

bool ZipVolume::exists(std::string_view path) const {
    if (path.empty() || path == "/")
        return true;
    std::string file_path = to_file_path(path);
    if (findEntry(file_path) != nullptr)
        return true;
    std::string dir_path = to_dir_path(path);
    if (findEntry(dir_path) != nullptr)
        return true;
    return false;
}

bool ZipVolume::isFile(std::string_view path) const {
    std::string file_path = to_file_path(path);
    const ZipEntry* entry = findEntry(file_path);
    return entry != nullptr && !entry->isDirectory;
}

bool ZipVolume::isDirectory(std::string_view _path) const {
    std::string dir_path = to_dir_path(_path);
    if (dir_path == "/")
        return true;
    const ZipEntry* entry = findEntry(dir_path);
    return entry != nullptr && entry->isDirectory;
}

static void item_entry(DirNode* st, const ZipEntry& entry, const std::string& name) {
    st->name = name;
    st->type = entry.isDirectory ? FileType::Directory : FileType::Regular;
    st->size = entry.uncompressedSize;
    st->epochSeconds(entry.modifiedTime);
    st->accessTime(std::chrono::system_clock::from_time_t(
        entry.accessTime != 0 ? entry.accessTime : entry.modifiedTime));
    st->creationTime(std::chrono::system_clock::from_time_t(
        entry.creationTime != 0 ? entry.creationTime : entry.modifiedTime));
    if (entry.hasUnixIds) {
        st->uid = static_cast<uint32_t>(entry.uid);
        st->gid = static_cast<uint32_t>(entry.gid);
    }
    st->mode = S_IRUSR | S_IRGRP | S_IROTH;
    if (st->type == FileType::Directory)
        st->mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    else
        st->mode |= S_IFREG;
}

void ZipVolume::readDir_inplace(DirNode& context, std::string_view path, bool recursive) {
    std::string dir_path = to_dir_path(path);
    std::string prefix = (dir_path == "/") ? "" : dir_path;

    for (const auto& pair : m_entries) {
        const std::string& entryName = pair.first;

        if (entryName.find(prefix) != 0) {
            continue;
        }

        std::string path_info = entryName.substr(prefix.length());
        if (path_info.empty()) // skip self entry
            continue;

        if (path_info.back() == '/')
            path_info = path_info.substr(0, path_info.length() - 1);

        size_t last_slash = path_info.find_last_of('/');
        DirNode* parent = &context;
        std::string base = path_info;

        if (last_slash != std::string::npos) {
            if (!recursive)
                continue;
            std::string dir = path_info.substr(0, last_slash);
            parent = context.resolve(dir, true, FileType::Directory);
            base = path_info.substr(last_slash + 1);
        }

        auto node = std::make_unique<DirNode>();
        item_entry(node.get(), pair.second, base);

        parent->putChild(std::move(node));
        parent->children_valid = true;
    } // for m_entries
}

bool ZipVolume::stat(std::string_view path, DirNode* status) const {
    std::string file_path = to_file_path(path);
    const ZipEntry* entry = findEntry(file_path);

    auto last_slash = file_path.find_last_of('/');
    std::string base =
        last_slash == std::string::npos ? file_path : file_path.substr(last_slash + 1);

    if (entry) {
        item_entry(status, *entry, base);
        return true;
    }

    std::string dir_path = to_dir_path(path);
    entry = findEntry(dir_path);
    if (entry) {
        item_entry(status, *entry, base);
        return true;
    }

    return false;
}

// std::unique_ptr<IReadStream> ZipVolume::openForRead(std::string_view path, std::string_view
// encoding) {
//     std::vector<uint8_t> data = readFile(path);
//     return
//     std::make_unique<DecodingReadStream>(std::make_unique<Uint8ArrayInputStream>(std::move(data)),
//     encoding);
// }

// std::unique_ptr<IWriteStream> ZipVolume::openForWrite(std::string_view path, bool append,
// std::string_view /*encoding*/) {
//     (void)path;
//     (void)append;
//     throw IOException("openForWrite", std::string(path), "ZipVolume is read-only");
// }

std::unique_ptr<InputStream> ZipVolume::newInputStream(std::string_view path) {
    std::string file_path = to_file_path(path);
    auto data = readFile(file_path);
    if (!data.has_value()) {
        throw IOException("newInputStream", std::string(file_path), "File is not readable");
    }
    return std::make_unique<Uint8ArrayInputStream>(std::move(*data));
}

std::unique_ptr<RandomInputStream> ZipVolume::newRandomInputStream(std::string_view path) {
    std::string file_path = to_file_path(path);
    auto data = readFile(file_path);
    if (!data.has_value()) {
        throw IOException("newRandomInputStream", std::string(file_path), "File is not readable");
    }
    return std::make_unique<Uint8ArrayInputStream>(std::move(*data));
}

std::unique_ptr<RandomReader> ZipVolume::newRandomReader(std::string_view path,
                                                         std::string_view encoding) {
    return Volume::newRandomReader(path, encoding);
}

std::unique_ptr<OutputStream> ZipVolume::newOutputStream(std::string_view path, bool append) {
    throw IOException("newOutputStream", std::string(path), "ZipVolume is read-only");
}

std::unique_ptr<Reader> ZipVolume::newReader(std::string_view path, std::string_view encoding) {
    std::string file_path = to_file_path(path);
    auto data = readFile(file_path);
    if (!data.has_value()) {
        throw IOException("newReader", std::string(file_path), "File is not readable");
    }
    // icu decode the data
    icu::UnicodeString unicodeString = icu::UnicodeString::fromUTF8(
        icu::StringPiece(reinterpret_cast<const char*>(data->data()), data->size()));
    std::string text;
    unicodeString.toUTF8String(text);
    return std::make_unique<StringReader>(text);
}

std::unique_ptr<Writer> ZipVolume::newWriter(std::string_view path, bool append,
                                             std::string_view encoding) {
    throw IOException("newWriter", std::string(path), "ZipVolume is read-only");
}

std::optional<std::vector<uint8_t>> ZipVolume::readFileUnchecked(std::string_view path, int64_t off,
                                                                 size_t len) {
    std::string file_path = to_file_path(path);
    const ZipEntry* entry = findEntry(file_path);
    if (!entry || entry->isDirectory) {
        // throw IOException("readFile", file_path, "File not found or is directory");
        return std::nullopt;
    }
    auto data = decompressEntry(*entry);

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

void ZipVolume::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    (void)path; // Suppress unused parameter warning
    (void)data;
    throw IOException("writeFile", std::string(path), "ZipVolume is read-only");
}

void ZipVolume::createDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("createDirectory", std::string(path), "ZipVolume is read-only");
}

void ZipVolume::removeDirectoryThrowsUnchecked(std::string_view path) {
    throw IOException("removeDirectory", std::string(path), "ZipVolume is read-only");
}

void ZipVolume::removeFileThrowsUnchecked(std::string_view path) {
    throw IOException("removeFile", std::string(path), "ZipVolume is read-only");
}

void ZipVolume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    (void)src; // Suppress unused parameter warning
    (void)dest;
    throw IOException("copyFile", std::string(src), "ZipVolume is read-only");
}

void ZipVolume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    (void)src; // Suppress unused parameter warning
    (void)dest;
    throw IOException("moveFile", std::string(src), "ZipVolume is read-only");
}

void ZipVolume::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) {
    (void)oldPath; // Suppress unused parameter warning
    (void)newPath;
    throw IOException("rename", std::string(oldPath), "ZipVolume is read-only");
}

std::optional<std::string> ZipVolume::getLocalFile(std::string_view path) const {
    (void)path;          // Suppress unused parameter warning
    return std::nullopt; // ZipVolume doesn't map to local files
}

std::string ZipVolume::getTempDir() {
    throw IOException("getTempDir", "", "ZipVolume is read-only");
}

std::string ZipVolume::createTempFile(std::string_view prefix, std::string_view suffix) {
    (void)prefix; // Suppress unused parameter warning
    (void)suffix;
    throw IOException("createTempFile", "", "ZipVolume is read-only");
}

void ZipVolume::parseZip() {
    m_entries.clear();
    if (!m_device) {
        return;
    }

    const uint64_t zipSize = m_device->size();
    if (zipSize < sizeof(ZipEndOfCentralDir))
        return;

    // EOCD is usually within last 64KB; also include EOCD itself.
    const uint64_t tailSize = std::min<uint64_t>(zipSize, 65536ull + sizeof(ZipEndOfCentralDir));
    const uint64_t tailStart = zipSize - tailSize;

    std::vector<uint8_t> tail(tailSize);
    if (!m_device->read(tailStart, tail.data(), tail.size())) {
        return;
    }

    size_t eocdPos = std::string::npos;
    for (size_t i = 0; i + sizeof(ZipEndOfCentralDir) <= tail.size(); ++i) {
        const uint32_t sig = readLE32(tail.data() + i);
        if (sig == 0x06054b50) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos == std::string::npos)
        return;

    const uint8_t* eocdPtr = tail.data() + eocdPos;
    const uint32_t cdSize = readLE32(eocdPtr + 12);
    const uint32_t cdOffset32 = readLE32(eocdPtr + 16);
    const uint16_t totalRecords = readLE16(eocdPtr + 10);

    const uint64_t cdOffset = cdOffset32;
    if (cdOffset + cdSize > zipSize)
        return;

    std::vector<uint8_t> cd(cdSize);
    if (cdSize > 0 && !m_device->read(cdOffset, cd.data(), cd.size())) {
        return;
    }

    const size_t centralEntryFixedSize = sizeof(ZipCentralDirEntry);
    size_t pos = 0;
    for (uint16_t i = 0; i < totalRecords; ++i) {
        if (pos + centralEntryFixedSize > cd.size())
            break;
        if (readLE32(cd.data() + pos) != 0x02014b50)
            break;

        // Parse fixed part (little-endian).
        const uint16_t compression = readLE16(cd.data() + pos + 10);
        const uint16_t modTime = readLE16(cd.data() + pos + 12);
        const uint16_t modDate = readLE16(cd.data() + pos + 14);
        const uint32_t compressedSize = readLE32(cd.data() + pos + 20);
        const uint32_t uncompressedSize = readLE32(cd.data() + pos + 24);
        const uint16_t filenameLen = readLE16(cd.data() + pos + 28);
        const uint16_t extraFieldLen = readLE16(cd.data() + pos + 30);
        const uint16_t commentLen = readLE16(cd.data() + pos + 32);
        const uint32_t localHeaderOffset = readLE32(cd.data() + pos + 42);

        const size_t filenameOffset = pos + centralEntryFixedSize;
        const size_t extraOffset = filenameOffset + filenameLen;
        const size_t commentOffset = extraOffset + extraFieldLen;

        if (commentOffset + commentLen > cd.size())
            break;

        ZipEntry entry;
        entry.name =
            std::string(reinterpret_cast<const char*>(cd.data() + filenameOffset), filenameLen);
        entry.localHeaderOffset = localHeaderOffset;
        entry.compressedSize = compressedSize;
        entry.uncompressedSize = uncompressedSize;
        entry.compressionMethod = compression;

        ZipExtraParsed extra{};
        if (extraFieldLen > 0 && extraOffset + extraFieldLen <= cd.size()) {
            scanZipCentralExtraField(cd.data() + extraOffset, extraFieldLen, &extra);
        }
        entry.modifiedTime =
            extra.hasUtMtime ? extra.utMtime : dosDateTimeToTimeT(modTime, modDate);
        entry.accessTime = extra.hasUtAtime ? extra.utAtime : 0;
        entry.creationTime = extra.hasUtCtime ? extra.utCtime : 0;
        if (extra.hasUx) {
            entry.hasUnixIds = true;
            entry.uid = extra.uid;
            entry.gid = extra.gid;
        }
        entry.isDirectory = (!entry.name.empty() && entry.name.back() == '/');

        m_entries[entry.name] = entry;

        pos = commentOffset + commentLen;
        if (pos >= cd.size())
            break;
    }
}

const ZipEntry* ZipVolume::findEntry(const std::string& path) const {
    auto it = m_entries.find(path);
    if (it != m_entries.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<uint8_t> ZipVolume::decompressEntry(const ZipEntry& entry) const {
    if (!m_device) {
        throw IOException("decompressEntry", entry.name, "Zip device is null");
    }

    const uint64_t zipSize = m_device->size();
    if (entry.localHeaderOffset + sizeof(ZipLocalFileHeader) > zipSize) {
        throw IOException("decompressEntry", entry.name, "Invalid ZIP entry offset");
    }

    std::vector<uint8_t> lfhBuf(sizeof(ZipLocalFileHeader));
    if (!m_device->read(entry.localHeaderOffset, lfhBuf.data(), lfhBuf.size())) {
        throw IOException("decompressEntry", entry.name, "Failed reading local header");
    }
    const uint32_t sig = readLE32(lfhBuf.data() + 0);
    if (sig != 0x04034b50) {
        throw IOException("decompressEntry", entry.name, "Invalid local file header");
    }
    const uint16_t filenameLen = readLE16(lfhBuf.data() + 26);
    const uint16_t extraFieldLen = readLE16(lfhBuf.data() + 28);

    const uint64_t dataOffset =
        entry.localHeaderOffset + sizeof(ZipLocalFileHeader) + filenameLen + extraFieldLen;
    if (dataOffset + entry.compressedSize > zipSize) {
        throw IOException("decompressEntry", entry.name, "Invalid ZIP entry size");
    }

    if (entry.compressionMethod == 0) {
        // Stored (no compression) - just copy the data.
        const size_t outSize = entry.uncompressedSize;
        std::vector<uint8_t> data(outSize);
        if (!m_device->read(dataOffset, data.data(), data.size())) {
            throw IOException("decompressEntry", entry.name, "Failed reading stored data");
        }
        return data;
    }

    if (entry.compressionMethod == 8) {
        // Deflate compression - read compressed bytes then inflate with zlib.
        std::vector<uint8_t> compressed(entry.compressedSize);
        if (!m_device->read(dataOffset, compressed.data(), compressed.size())) {
            throw IOException("decompressEntry", entry.name, "Failed reading compressed data");
        }

        z_stream zs;
        memset(&zs, 0, sizeof(zs));
        if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
            throw IOException("decompressEntry", entry.name, "Failed to initialize zlib");
        }

        zs.next_in = compressed.data();
        zs.avail_in = static_cast<unsigned int>(compressed.size());

        std::vector<uint8_t> data(entry.uncompressedSize);
        zs.next_out = data.data();
        zs.avail_out = static_cast<unsigned int>(data.size());

        int ret = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);

        if (ret != Z_STREAM_END) {
            throw IOException("decompressEntry", entry.name, "Failed to decompress");
        }
        return data;
    }

    throw IOException("decompressEntry", entry.name, "Unsupported compression method");
}
