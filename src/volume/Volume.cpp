#include "Volume.hpp"

#include "VolumeFile.hpp"

#include "../io/IOException.hpp"
#include "../io/InputStream.hpp"
#include "../io/PrintStream.hpp"
#include "../io/ReversedReader.hpp"
#include "../io/StringReader.hpp"
#include "../io/U32stringReader.hpp"
#include "../util/unicode.hpp"

#include <bas/log/uselog.h>

#include <algorithm>
#include <cassert>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <sys/stat.h>

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

std::string Volume::readRCFile(std::string_view name) {
    std::unique_ptr<VolumeFile> rcDir = resolve("/.rc");
    try {
        if (!rcDir->exists()) {
            return getDefaultLabel();
        }
        std::string content = rcDir->resolve(name)->readFileString();
        // get the first line
        std::string line = content.substr(0, content.find('\n'));
        // left trim (spaces and tabs)
        line = line.erase(0, line.find_first_not_of(' '));
        // right trim
        line = line.erase(line.find_last_not_of(' ') + 1);
        return line;
    } catch (...) {
        return getDefaultLabel();
    }
}

bool Volume::writeRCFile(std::string_view name, std::string_view data) {
    std::unique_ptr<VolumeFile> rcDir = resolve("/.rc");
    try {
        if (!rcDir->exists())
            rcDir->createDirectories();
        rcDir->resolve(name)->writeFileString(data);
        return true;
    } catch (...) {
        return false;
    }
}

std::string Volume::getTypeString() const { return volumeTypeToString(getType()); }

std::string Volume::getUUID() {
    if (m_uuid_cached)
        return m_uuid;
    m_uuid = readRCFile(m_uuidFile);
    m_uuid_cached = true;
    return m_uuid;
}

void Volume::setUUIDForced(std::string_view u) {
    writeRCFile(m_uuidFile, u);
    m_uuid = std::string(u);
    m_uuid_cached = true;
}

std::string Volume::getSerial() {
    if (m_serial_cached)
        return m_serial;
    m_serial = readRCFile(m_serialFile);
    m_serial_cached = true;
    return m_serial;
}

void Volume::setSerialForced(std::string_view s) {
    writeRCFile(m_serialFile, s);
    m_serial = std::string(s);
    m_serial_cached = true;
}

std::string Volume::getLabel() {
    if (m_label_cached)
        return m_label;
    m_label = readRCFile(m_labelFile);
    m_label_cached = true;
    return m_label;
}

void Volume::setLabel(std::string_view label) {
    if (writeRCFile(m_labelFile, label)) {
        m_label = label;
        m_label_cached = true;
    } else {
        logerror_fmt("Failed to write to LABEL file");
    }
}

std::unique_ptr<VolumeFile> Volume::getRootFile() {
    return std::make_unique<VolumeFile>(this, std::string("/"));
}

std::unique_ptr<VolumeFile> Volume::resolve(std::string_view path) {
    return std::make_unique<VolumeFile>(this, std::string(path));
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

std::vector<std::unique_ptr<FileStatus>> Volume::readDir(std::string_view path, bool recursive) {
    if (path.empty())
        throw std::invalid_argument("Volume::readDir: path is required");
    std::vector<std::unique_ptr<FileStatus>> list;
    readDir_inplace(list, path, recursive);
    return list;
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
            throw IOException("createDirectoryThrows", path, "Directory already exists");
        } else {
            throw IOException("createDirectoryThrows", path, "File with same name exists");
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
        if (!isDirectory(path))
            return false;
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
            throw IOException("createDirectories", std::string(path), "Path is not a directory");
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
        throw IOException("removeDirectory", std::string(path), "Path is not a directory");
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
        throw IOException("removeDirectory", path, "Path does not exist");
    if (!isDirectory(path))
        throw IOException("removeDirectory", path, "Path is not a directory");
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
        throw IOException("removeFile", path, "Path does not exist");
    if (!isFile(path))
        throw IOException("removeFile", path, "Path is not a file");
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
        throw IOException("copyFile", std::string(src), "Source file does not exist");
    if (!isFile(src))
        throw IOException("copyFile", std::string(src), "Source path is not a file");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite)
            removeFile(dest);
        else
            throw IOException("copyFile", std::string(dest), "Destination file already exists");
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
        throw IOException("moveFile", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw IOException("moveFile", std::string(dest),
                                  "Failed to remove destination file");
        } else {
            throw IOException("moveFile", std::string(dest), "Destination file already exists");
        }
    }

    moveFileThrowsUnchecked(src, dest);
}

bool Volume::rename(std::string_view _src, std::string_view _dest, bool overwrite) {
    std::string src = normalizeArg(_src);
    std::string dest = normalizeArg(_dest);
    if (!exists(src))
        throw IOException("rename", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw IOException("rename", std::string(dest), "Failed to remove destination file");
        } else {
            throw IOException("rename", std::string(dest), "Destination file already exists");
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
        throw IOException("rename", std::string(src), "Source file does not exist");

    if (isDirectory(dest)) {
        size_t last_slash = src.find_last_of('/');
        std::string base = last_slash == std::string::npos ? src : src.substr(last_slash + 1);
        dest = dest + "/" + base;
    }

    if (exists(dest)) {
        if (overwrite) {
            if (!removeFile(dest))
                throw IOException("rename", std::string(dest), "Failed to remove destination file");
        } else {
            throw IOException("rename", std::string(dest), "Destination file already exists");
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

std::vector<uint8_t> Volume::readFile(std::string_view _path) {
    if (!exists(_path))
        throw IOException("readFile", 
        getLabel() + "::" + std::string(_path), "Path does not exist");
    if (!isFile(_path))
        throw IOException("readFile", 
        getLabel() + "::" + std::string(_path), "Path is not a regular file");

    return readFileUnchecked(_path);
}

std::string Volume::readFileUTF8(std::string_view path) {
    if (path.empty())
        throw std::invalid_argument("Volume::readFileUTF8: path is required");
    std::vector<uint8_t> data = readFile(path);
    return std::string(data.begin(), data.end());
}

std::string Volume::readFileString(std::string_view path, std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::readFileString: path is required");
    std::vector<uint8_t> data = readFile(path);

    if (data.empty()) {
        return "";
    }

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

std::deque<std::string> Volume::readLines(std::string_view path, int maxLines,
                                          std::string_view encoding) {
    if (path.empty())
        throw std::invalid_argument("Volume::readLines: path is required");
    auto in = newReader(path, encoding);
    assert(in);
    if (!in)
        return {};

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
        throw IOException("writeFile", std::string(_path), //
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

std::string format_mode(int mode) {
    std::string s;
    s.reserve(10);

    // 1. File Type
    if (S_ISDIR(mode))
        s += 'd';
    else if (S_ISLNK(mode))
        s += 'l';
    else if (S_ISCHR(mode))
        s += 'c';
    else if (S_ISBLK(mode))
        s += 'b';
    else if (S_ISFIFO(mode))
        s += 'p';
    else if (S_ISSOCK(mode))
        s += 's';
    else
        s += '-';

    // 2. Owner Permissions
    s += (mode & S_IRUSR) ? 'r' : '-';
    s += (mode & S_IWUSR) ? 'w' : '-';
    s += (mode & S_IXUSR) ? 'x' : '-';

    // 3. Group Permissions
    s += (mode & S_IRGRP) ? 'r' : '-';
    s += (mode & S_IWGRP) ? 'w' : '-';
    s += (mode & S_IXGRP) ? 'x' : '-';

    // 4. Other Permissions
    s += (mode & S_IROTH) ? 'r' : '-';
    s += (mode & S_IWOTH) ? 'w' : '-';
    s += (mode & S_IXOTH) ? 'x' : '-';

    return s;
}

std::string format_size_human(size_t size) {
    static const std::vector<std::string> units = {"B", "K", "M", "G", "T", "P"};

    if (size < 1024) {
        return std::to_string(size) + units[0];
    }

    int unit_index = 0;
    double d_size = static_cast<double>(size);

    while (d_size >= 1024 && unit_index < units.size() - 1) {
        d_size /= 1024.0;
        unit_index++;
    }

    std::stringstream ss;
    // Set precision to 1 decimal place (e.g., 1.5G)
    ss << std::fixed << std::setprecision(1) << d_size << units[unit_index];
    return ss.str();
}

std::string format_size(size_t size, bool human) {
    if (human) {
        return format_size_human(size);
    }

    std::stringstream ss;
    // This injects commas (e.g., 1,234,567) using the user's locale
    ss.imbue(std::locale(""));
    ss << std::fixed << size;
    return ss.str();
}

std::string format_time(time_t time) {
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Volume::ls(std::string_view options, std::string_view path) {
    bool recursive = options.find('R') != std::string::npos;
    bool long_format = options.find('l') != std::string::npos;
    bool all = options.find('a') != std::string::npos;
    bool almost_all = options.find('A') != std::string::npos;
    bool format_suffix = options.find('F') != std::string::npos;
    bool human = options.find('h') != std::string::npos;
    bool color = options.find('C') != std::string::npos;

    std::string color_dir = "\033[34m";
    std::string color_link = "\033[32m";
    std::string color_fifo = "\033[33m";
    std::string color_socket = "\033[35m";
    std::string color_character = "\033[36m";
    std::string color_block = "\033[37m";
    std::string color_regular = "\033[39m";
    std::string color_regular_executable = "\033[32m";
    std::string color_regular_archive = "\033[33m";
    std::string color_suffix = "\033[0m";
    std::string color_end = color ? "\033[0m" : "";

    bool accessTime = false;
    bool createTime = false;

    std::vector<std::unique_ptr<FileStatus>> list = readDir(path, recursive);

    int max_width = 0;
    for (const auto& st : list) {
        int width = st->name.size();
        if (width > max_width)
            max_width = width;
    }
    max_width += path.size() + 1;

    const char* env_columns = getenv("COLUMNS");
    int columns = env_columns ? std::stoi(env_columns) : 80;
    int names_per_line = columns / (max_width + 1);

    int count_in_line = 0;
    for (const auto& st : list) {
        bool includes = st->name.front() != '.';
        if (!includes) {
            if (st->name == "." || st->name == "..")
                includes = all;
            else
                includes = all || almost_all;
            if (!includes)
                continue;
        }

        std::string suffix = "";
        std::string color_name = "";

        if (format_suffix || color) {
            color_name = color_regular;
            if (st->isDirectory()) {
                suffix = "/";
                color_name = color_dir;
            } else if (st->isSymbolicLink()) {
                suffix = "->";
                color_name = color_link;
            } else if (st->isFIFO()) {
                suffix = "|";
                color_name = color_fifo;
            } else if (st->isSocket()) {
                suffix = "=";
                color_name = color_socket;
            } else if (st->isCharacterDevice()) {
                suffix = "c";
                color_name = color_character;
            } else if (st->isBlockDevice()) {
                suffix = "b";
                color_name = color_block;
            } else if (st->isRegularFile()) {
                if (st->mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
                    suffix = "*";
                    color_name = color_regular;
                }
            }
            if (!format_suffix)
                suffix = "";
            if (!color)
                color_name = "";
        }

        std::string childPath(path);
        if (!childPath.empty())
            if (childPath.back() != '/')
                childPath += "/";
        childPath += st->name;

        if (long_format) {
            std::string mode = format_mode(st->mode);
            std::string size = format_size(st->size, human);
            std::cout << " " << std::setw(10) << mode //
                      << " " << std::setw(10) << size;

            std::string modifiedTime = format_time(st->modifiedTime);
            if (accessTime)
                std::string accessTime = format_time(st->accessTime);
            if (createTime)
                std::string createTime = format_time(st->createTime);
            std::cout << " " << color_name << childPath << color_end;
            if (format_suffix) {
                std::cout << color_suffix << suffix << color_end;
            }
            std::cout << std::endl;
        } else {
            std::cout << std::setw(max_width + 1) << color_name << childPath << color_end;
            if (format_suffix)
                std::cout << color_suffix << suffix << color_end;
            count_in_line++;
            if (count_in_line >= names_per_line) {
                std::cout << std::endl;
                count_in_line = 0;
            }
        }
    } // for
    if (count_in_line > 0) {
        std::cout << std::endl;
    }
    if (recursive) {
        for (const auto& st : list) {
            if (st->name == "." || st->name == "..")
                continue;
            if (st->name.front() == '.')
                if (!all && !almost_all)
                    continue;
            if (st->isDirectory()) {
                std::string childPath(path);
                if (!childPath.empty())
                    if (childPath.back() != '/')
                        childPath += "/";
                childPath += st->name;
                ls(options, childPath);
            }
        }
    }
}

void Volume::tree(std::string_view path) { tree(path, ""); }

void Volume::tree(std::string_view path, const std::string& prefix) {
    std::vector<std::unique_ptr<FileStatus>> list;
    try {
        list = this->readDir(path);
    } catch (const std::exception& e) {
        std::cerr << "Error reading directory " << path << ": " << e.what() << std::endl;
        return;
    }
    for (const auto& st : list) {
        if (!st)
            continue;
        bool isDir = st->isDirectory();
        std::string line = prefix;
        line += st->name;
        if (isDir)
            line += "/";
        if (st->isRegularFile() && st->size) {
            char buf[32];
            snprintf(buf, sizeof(buf), " (%zu)", static_cast<size_t>(st->size));
            line += buf;
        }
        std::puts(line.c_str());
        if (isDir) {
            std::string childPath(path);
            if (!childPath.empty())
                if (childPath.back() != '/')
                    childPath += "/";
            childPath += st->name;
            tree(childPath, prefix + "  ");
        }
    }
}
