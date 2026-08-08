#include "Volume.hpp"

#include "BlockDevice.hpp"
#include "VolumeExceptions.hpp"
#include "VolumeFile.hpp"

#include "../io/InputStream.hpp"
#include "../io/PrintStream.hpp"
#include "../io/ReversedReader.hpp"
#include "../io/StringReader.hpp"
#include "../io/U32stringReader.hpp"
#include "../security/PublicAccess.hpp"
#include "../util/unicode.hpp"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <bas/log/uselog.h>

#include <sys/stat.h>

Volume::~Volume() = default;

std::shared_ptr<bas::security::UserStore> Volume::getUserStore() {
    if (m_userStore)
        return m_userStore;
    return bas::security::PublicAccess::userStore();
}

std::shared_ptr<bas::security::PolicyStore> Volume::getPolicyStore() {
    if (m_policyStore)
        return m_policyStore;
    return bas::security::PublicAccess::policyStore();
}

void Volume::setUserStore(std::shared_ptr<bas::security::UserStore> store) {
    m_userStore = std::move(store);
}

void Volume::setPolicyStore(std::shared_ptr<bas::security::PolicyStore> store) {
    m_policyStore = std::move(store);
}

std::string volumeTypeToString(VolumeType t) {
    switch (t) {
    case VolumeType::HARDDISK:
        return "HARDDISK";
    case VolumeType::FLOPPY:
        return "FLOPPY";
    case VolumeType::CDROM:
        return "CDROM";
    case VolumeType::NETWORK:
        return "NETWORK";
    case VolumeType::ARCHIVE:
        return "ARCHIVE";
    case VolumeType::SYSTEM:
        return "SYSTEM";
    case VolumeType::MEMORY:
        return "MEMORY";
    case VolumeType::OTHER:
        return "OTHER";
    default:
        return "UNKNOWN";
    }
}

std::string Volume::getTypeString() const { return volumeTypeToString(getType()); }

std::string Volume::getDeviceUuid() {
    try {
        auto dev = BlockDevice::load(getDeviceUrl());
        return dev->uuid();
    } catch (const VolumeException& e) {
        logerror_fmt("Failed to get block device: {}", e.what());
        return "";
    }
}

std::string Volume::getUuid() {
    if (c_uuid_valid)
        return c_uuid;
    c_uuid = readUuid();
    c_uuid_valid = true;
    return c_uuid;
}

void Volume::setUuid(std::string_view s) {
    if (writeUuid(s)) {
        c_uuid = s;
        c_uuid_valid = true;
    }
    else {
        logerror_fmt("Failed to change volume uuid");
    }
}

std::string Volume::getLabel() {
    if (c_label_valid)
        return c_label;
    c_label = readLabel();
    if (c_label.empty()) {
        c_label = getDefaultLabel();
    }
    c_label_valid = true;
    return c_label;
}

void Volume::setLabel(std::string_view label) {
    if (writeLabel(label)) {
        c_label = label;
        c_label_valid = true;
    } else {
        logerror_fmt("Failed to change volume label");
    }
}

std::string Volume::normalizeArg(std::string_view path, std::optional<std::string> fallback) const {
    if (path.empty()) {
        if (fallback) {
            return *fallback;
        } else {
            throw std::invalid_argument("Volume::normalizeArg: path is mandatory but it's empty");
        }
    }
    return normalize(path);
}

std::string Volume::normalize(std::string_view path) const {
    assert(!path.empty());
    while (!path.empty() && path.back() == '/') {
        path = path.substr(0, path.size() - 1);
    }

    while (!path.empty() && path.front() == '/') {
        path = path.substr(1);
    }
    if (path.empty()) {
        return "/";
    }
    // process path by tokens (split by /), so
    //  skip empty tokens
    //  skip . tokens
    //  skip .. tokens with parent directory
    std::vector<std::string_view> tokens;
    size_t len = path.size();
    size_t pos = 0;
    size_t slash = path.find('/');
    while (pos < len) {
        std::string_view token;
        if (slash == std::string::npos) {
            token = path.substr(pos);
            pos = len;
        } else {
            token = path.substr(pos, slash - pos);
            pos = slash + 1;
            slash = path.find('/', pos);
        }

        if (token == "." || token.empty()) {
            // skip
        } else if (token == "..") {
            if (!tokens.empty()) {
                tokens.pop_back();
                // skip
            } else {
                // block out-of-volume access
            }
        } else {
            tokens.push_back(token);
        }
    }
    std::string result = "";
    for (const auto& token : tokens) {
        result += "/";
        result += token;
    }
    return result;
}

std::string Volume::toRealPath(std::string_view path) const { return normalizeArg(path); }

std::unique_ptr<DirNode> Volume::readDir(std::string_view path, bool recursive) {
    std::unique_ptr<DirNode> root = std::make_unique<DirNode>();
    readDir_inplace(*root, path, recursive);
    return root;
}

// wrapper for createDirectoryThrows
bool Volume::createDirectory(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (path.empty() || path == "/")
        return false;
    if (exists(path)) {
        return false;
    }
    try {
        createDirectoryThrowsUnchecked(path);
    } catch (...) {
        return false;
    }
    return true;
}

void Volume::createDirectoryThrows(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (path.empty() || path == "/")
        throw std::invalid_argument("Volume::createDirectoryThrows: path is root");
    if (exists(path)) {
        if (isDirectory(path)) {
            throw VolumeException(this, "createDirectoryThrows", path, "Directory already exists");
        } else {
            throw VolumeException("createDirectoryThrows", path, "File with same name exists");
        }
    }
    createDirectoryThrowsUnchecked(path);
}

bool Volume::createDirectories(std::string_view _path) {
    if (_path.empty())
        return false;
    std::string path = normalizeArg(_path);
    if (path.empty() || path == "/") {
        return false;
    }

    if (exists(path)) {
        return isDirectory(path);
    }

    std::vector<std::string> mkdir_list;
    while (!isDirectory(path)) {
        mkdir_list.push_back(path);
        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos)
            break;
        path = path.substr(0, last_slash);
        if (path.empty() || path == "/")
            break;
    }

    if (mkdir_list.empty())
        return false;

    // reverse iterate and create directories
    for (auto it = mkdir_list.rbegin(); it != mkdir_list.rend(); ++it) {
        if (!createDirectory(*it))
            return false;
    }
    return true;
}

void Volume::createDirectoriesThrows(std::string_view _path) {
    if (_path.empty())
        throw std::invalid_argument("Volume::createDirectories: path is required");

    std::string path = normalizeArg(_path);
    if (path.empty())
        throw std::invalid_argument("Volume::createDirectories: path is empty");
    if (path == "/")
        return;

    if (exists(path)) {
        if (!isDirectory(path))
            throw VolumeException(this, "createDirectories", std::string(path),
                                  "Path is not a directory");
    }

    std::vector<std::string> mkdir_list;
    while (!isDirectory(path)) {
        mkdir_list.push_back(path);
        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos)
            break;
        path = path.substr(0, last_slash);
        if (path.empty() || path == "/")
            break;
    }

    if (mkdir_list.empty())
        return;

    // reverse iterate and create directories
    for (auto it = mkdir_list.rbegin(); it != mkdir_list.rend(); ++it) {
        createDirectoryThrowsUnchecked(*it);
    }
}

bool Volume::createParentDirectories(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (path.empty() || path == "/")
        return false;
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos)
        return false;
    std::string parent = path.substr(0, last_slash);
    return createDirectories(parent);
}

bool Volume::removeDirectory(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (!exists(path))
        return false;
    if (!isDirectory(path))
        throw VolumeException(this, "removeDirectory", std::string(path),
                              "Path is not a directory");
    try {
        removeDirectoryThrows(path);
    } catch (...) {
        return false;
    }
    return true;
}

void Volume::removeDirectoryThrows(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (!exists(path))
        throw VolumeException(this, "removeDirectory", path, "Path does not exist");
    if (!isDirectory(path))
        throw VolumeException(this, "removeDirectory", path, "Path is not a directory");
    removeDirectoryThrowsUnchecked(path);
}

bool Volume::removeFile(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (!exists(path))
        return false;
    if (!isFile(path))
        return false;
    try {
        removeFileThrows(path);
    } catch (...) {
        return false;
    }
    return true;
}

void Volume::removeFileThrows(std::string_view _path) {
    std::string path = normalizeArg(_path);
    if (!exists(path))
        throw VolumeException(this, "removeFile", path, "Path does not exist");
    if (!isFile(path))
        throw VolumeException(this, "removeFile", path, "Path is not a file");
    removeFileThrowsUnchecked(path);
}

bool Volume::copyFile(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        return false;
    if (!isFile(src))
        return false;

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite)
            removeFile(dest);
        else
            return false;
    }
    try {
        copyFileThrowsUnchecked(src, dest);
        return true;
    } catch (...) {
        return false;
    }
}

void Volume::copyFileThrows(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        throw VolumeException(this, "copyFile", std::string(src), "Source file does not exist");
    if (!isFile(src))
        throw VolumeException(this, "copyFile", std::string(src), "Source path is not a file");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite)
            removeFile(dest);
        else
            throw VolumeException(this, "copyFile", std::string(dest),
                                  "Destination file already exists");
    }
    copyFileThrowsUnchecked(src, dest);
}

bool Volume::moveFile(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        return false;

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                return false;
        } else {
            return false;
        }
    }

    try {
        moveFileThrowsUnchecked(src, dest);
        return true;
    } catch (...) {
        return false;
    }
}

void Volume::moveFileThrows(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        throw VolumeException(this, "moveFile", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw VolumeException(this, "moveFile", std::string(dest),
                                      "Failed to remove destination file");
        } else {
            throw VolumeException(this, "moveFile", std::string(dest),
                                  "Destination file already exists");
        }
    }

    moveFileThrowsUnchecked(src, dest);
}

bool Volume::rename(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        throw VolumeException(this, "rename", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw VolumeException(this, "rename", std::string(dest),
                                      "Failed to remove destination file");
        } else {
            throw VolumeException(this, "rename", std::string(dest),
                                  "Destination file already exists");
        }
    }
    try {
        renameFileThrowsUnchecked(src, dest);
        return true;
    } catch (...) {
        return false;
    }
}

void Volume::renameFileThrows(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        throw VolumeException(this, "rename", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw VolumeException(this, "rename", std::string(dest),
                                      "Failed to remove destination file");
        } else {
            throw VolumeException(this, "rename", std::string(dest),
                                  "Destination file already exists");
        }
    }
    renameFileThrowsUnchecked(src, dest);
}
/**
 * @param buf the buffer to store the compressed string
 * @param buf_size include the null terminator
 * @param src the source string
 * @param ellipsis the ellipsis string
 * @return the buffer pointer
 */
char* ellpsis(char* buf, size_t buf_size, const char* src, const char* ellipsis = "...") {
    int src_len = static_cast<int>(strlen(src));
    int ellipsis_len = static_cast<int>(strlen(ellipsis));
    int maxchars = std::min<int>(buf_size - ellipsis_len - 1, src_len);
    memcpy(buf, src, maxchars);
    if (maxchars < src_len) {
        memcpy(buf + maxchars, ellipsis, ellipsis_len);
        buf[buf_size - 1] = '\0';
    } else {
        buf[maxchars] = '\0';
    }
    return buf;
}

std::unique_ptr<Reader> Volume::newReader(std::string_view path, std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::newReader: path is required");
    auto in = newInputStream(path);
    if (!in)
        return nullptr;
    auto data = in->readBytesUntilEOF();
    in->close();

    if (get_loglevel() >= LOG_LEVEL_INFO) {
        int size = static_cast<int>(data.size());
        unsigned char* content = data.data();

        char header_ellipsis[80];
        ellpsis(header_ellipsis, sizeof(header_ellipsis), (const char*)content);

        loginfo_fmt("volumn read: %d bytes => %s.", size, header_ellipsis);
    }

    std::string text;
    if (data.empty())
        text = "";
    else if (encoding == "UTF-8" || encoding.empty())
        text = std::string(data.begin(), data.end());
    else {
        auto unicode = unicode::fromEncoding(data, std::string(encoding));
        text = unicode::convertToUTF8(unicode);
    }
    return std::make_unique<StringReader>(text);
}

std::unique_ptr<Writer> Volume::newWriter(std::string_view path, bool append,
                                          std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::newWriter: path is required");
    auto out = newOutputStream(path, append);
    if (!out)
        return nullptr;
    return std::make_unique<PrintStream>(std::move(out), encoding);
}

std::unique_ptr<RandomReader> Volume::newRandomReader(std::string_view path,
                                                      std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::newRandomReader: path is required");
    auto in = newRandomInputStream(path);
    if (!in)
        return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();
    icu::UnicodeString unicode = unicode::fromEncoding(data, encoding);
    std::u32string u32 = unicode::convertToU32(unicode);
    return std::make_unique<U32stringReader>(u32);
}

std::vector<uint8_t> Volume::readFile(std::string_view _path, int64_t off, size_t len) {
    if (!exists(_path)) {
        throw VolumeException(this, "readFile", std::string(_path), "Path does not exist");
    }
    if (!isFile(_path)) {
        throw VolumeException(this, "readFile", std::string(_path), "Path is not a regular file");
    }
    return readFileUnchecked(_path, off, len);
}

std::vector<uint8_t> Volume::readFile(std::string_view path, std::vector<uint8_t> default_data,
                                      int64_t off, size_t len) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFile(path, off, len);
    } catch (...) {
        return default_data;
    }
}

std::optional<std::vector<uint8_t>>
Volume::readFileOpt(std::string_view path, int64_t off, size_t len,
                    std::optional<std::vector<uint8_t>> default_data) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFile(path, off, len);
    } catch (...) {
        return default_data;
    }
}

std::string Volume::readFileUTF8(std::string_view path) {
    if (path.empty())
        throw std::invalid_argument("Volume::readFileUTF8: path is required");

    if (!exists(path)) {
        throw VolumeException(this, "readFileUTF8", std::string(path), "Path does not exist");
    }
    if (!isFile(path)) {
        throw VolumeException(this, "readFileUTF8", std::string(path),
                              "Path is not a regular file");
    }

    auto data = readFile(path);
    return std::string(data.begin(), data.end());
}

std::string Volume::readFileUTF8(std::string_view path, std::string default_data) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFileUTF8(path);
    } catch (...) {
        return default_data;
    }
}

std::optional<std::string> Volume::readFileUTF8Opt(std::string_view path,
                                                   std::optional<std::string> default_data) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFileUTF8(path);
    } catch (...) {
        return default_data;
    }
}

std::string Volume::readFileString(std::string_view path, std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::readFileString: path is required");

    if (!exists(path)) {
        throw VolumeException(this, "readFileString", std::string(path), "Path does not exist");
    }
    if (!isFile(path)) {
        throw VolumeException(this, "readFileString", std::string(path),
                              "Path is not a regular file");
    }

    auto data = readFile(path);
    if (data.empty())
        return "";

    // For UTF-8 encoding (default), convert bytes to string directly
    std::string encStr(encoding);
    if (encStr == "UTF-8" || encStr.empty()) {
        return std::string(data.begin(), data.end());
    }

    // Convert from specified encoding to UTF-8
    auto unicode = unicode::fromEncoding(data, encStr);
    auto utf8 = unicode::convertToUTF8(unicode);
    return utf8;
}

std::string Volume::readFileString(std::string_view path, std::string default_data,
                                   std::string_view encoding) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFileString(path, encoding);
    } catch (...) {
        return default_data;
    }
}

std::optional<std::string> Volume::readFileStringOpt(std::string_view path,
                                                     std::optional<std::string> default_data,
                                                     std::string_view encoding) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readFileString(path, encoding);
    } catch (...) {
        return default_data;
    }
}

std::deque<std::string> Volume::readLines(std::string_view path, int maxLines,
                                          std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::readLines: path is required");

    auto in = newReader(path, encoding);
    if (in == nullptr)
        throw VolumeException(this, "readLines", std::string(path), "Failed to create reader");

    std::deque<std::string> lines;
    std::string line;
    int lineCount = 0;
    while ((line = in->readLine()) != "") {
        if (maxLines >= 0 && lineCount >= maxLines) {
            break;
        }
        lines.push_back(line);
        lineCount++;
    }

    return lines;
}

std::deque<std::string> Volume::readLines(std::string_view path,
                                          std::deque<std::string> default_data, int maxLines,
                                          std::string_view encoding) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readLines(path, maxLines, encoding);
    } catch (...) {
        return default_data;
    }
}

std::optional<std::deque<std::string>>
Volume::readLinesOpt(std::string_view path, std::optional<std::deque<std::string>> default_data,
                     int maxLines, std::string_view encoding) {
    if (!exists(path) || !isFile(path))
        return default_data;
    try {
        return this->readLines(path, maxLines, encoding);
    } catch (...) {
        return default_data;
    }
}

std::deque<std::string> Volume::readLastLines(std::string_view path, int maxLines,
                                              std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::readLastLines: path is required");
    auto in = newRandomInputStream(path);
    if (!in)
        return {};

    if (!in->seek(0, std::ios::end))
        return {};

    int64_t fileSize = in->position();
    if (fileSize <= 0)
        return {};

    // Helper state: stream + logical position for backward read (starts at end)
    struct Source {
        RandomInputStream* stream;
        int64_t pos;

        size_t read_backward(uint8_t* buf, size_t end, size_t len) {
            size_t toRead = static_cast<size_t>(std::min<int64_t>(len, pos));
            if (toRead == 0)
                return 0;
            if (!stream->seek(pos - static_cast<int64_t>(toRead), std::ios::beg))
                return 0;
            size_t n = stream->read(buf + end - toRead, 0, static_cast<size_t>(toRead));
            if (n == 0)
                return 0;
            pos -= static_cast<int64_t>(n);
            return n;
        }
    };
    Source state{in.get(), fileSize};

    const int limit = (maxLines >= 0) ? maxLines : 1000;
    read_backward_func_t read_backward = [&state](uint8_t* buf, size_t end, size_t len) {
        return state.read_backward(buf, end, len);
    };
    ReversedReader reader(read_backward);
    std::vector<std::string> revLines = reader.readLines(encoding, limit);
    return std::deque<std::string>(revLines.rbegin(), revLines.rend());
}

void Volume::writeFile(std::string_view _path, const std::vector<uint8_t>& data) {
    if (exists(_path) && !isFile(_path))
        throw VolumeException(this, "writeFile", std::string(_path), //
                              "Path is not a regular file: " + std::string(_path));
    writeFileUnchecked(_path, data);
}

void Volume::writeFileUTF8(std::string_view path, std::string_view data) {
    if (path.empty())
        throw std::invalid_argument("Volume::writeFileUTF8: path is required");
    std::vector<uint8_t> bytes(data.begin(), data.end());
    writeFile(path, bytes);
}

void Volume::writeFileString(std::string_view path, std::string_view data,
                             std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::writeFileString: path is required");
    // For UTF-8 encoding (default), convert string to bytes directly
    std::string encStr(encoding);
    if (encoding == "UTF-8" || encoding.empty()) {
        std::vector<uint8_t> bytes(data.begin(), data.end());
        writeFile(path, bytes);
        return;
    }

    // Convert from UTF-8 (internal string representation) to specified encoding
    std::vector<uint8_t> utf8Data(data.begin(), data.end());
    auto unicode = unicode::fromEncoding(utf8Data, "UTF-8");
    auto converted = unicode::toEncoding(unicode, encStr);

    // Write the converted bytes
    writeFile(path, converted);
}

void Volume::writeLines(std::string_view path, const std::deque<std::string>& lines,
                        std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::writeLines: path is required");
    std::ostringstream content;
    for (const auto& line : lines) {
        content << line << "\n";
    }
    writeFileString(path, content.str(), encoding);
}

void Volume::writeLines(std::string_view path, const std::vector<std::string>& lines,
                        std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::writeLines: path is required");
    std::ostringstream content;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            // Add newline after each line except the last
            content << "\n";
        }
        content << lines[i];
    }

    writeFileString(path, content.str(), encoding);
}

std::optional<std::string> Volume::getSoftConfigPath(SoftConfigUse use) {
    switch (use) {
        case SoftConfigUse::UUID:
            return "/.rc/UUID";
        case SoftConfigUse::LABEL:
            return "/.rc/LABEL";
        default:
            return std::nullopt;
    }
}

std::string Volume::readUuid() {
    auto path = getSoftConfigPath(SoftConfigUse::UUID);
    if (!path) {
        return "";
    }
    return readFileUTF8(*path);
}

bool Volume::writeUuid(std::string_view uuid) {
    auto path = getSoftConfigPath(SoftConfigUse::UUID);
    if (!path) {
        return false;
    }
    writeFileUTF8(*path, uuid);
    return true;
}

std::string Volume::readLabel() {
    auto path = getSoftConfigPath(SoftConfigUse::LABEL);
    if (!path) {
        return "";
    }
    return readFileUTF8(*path);
}

bool Volume::writeLabel(std::string_view label) {
    auto path = getSoftConfigPath(SoftConfigUse::LABEL);
    if (!path) {
        return false;
    }
    writeFileUTF8(*path, label);
    return true;
}
