#include "Volume.hpp"

#include "VolumeFile.hpp"

#include "../io/IOException.hpp"
#include "../io/InputStream.hpp"
#include "../io/PrintStream.hpp"
#include "../io/ReversedReader.hpp"
#include "../io/StringReader.hpp"
#include "../io/U32stringReader.hpp"
#include "../util/unicode.hpp"

#include <log/uselog.h>

#include <cassert>
#include <deque>
#include <sstream>
#include <stdexcept>

// throws if conversion fails and how is boost::locale::conv::stop
static std::vector<uint8_t> convertEncoding(const std::vector<uint8_t>& data, 
                                   std::string_view fromEncoding, 
                                   std::string_view toEncoding,
                                   unicode_conversion_mode convertion = UNICODE_ERROR, 
                                   char32_t replacement = U'\uFFFD') {
    if (data.empty()) {
        return {};
    }
    
    // If source and target are the same, no conversion needed
    if (fromEncoding == toEncoding) {
        return data;
    }
    
    // Convert using ICU
    auto unicode = unicode::fromEncoding(data, fromEncoding, convertion, replacement);
    return unicode::toEncoding(unicode, toEncoding);
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

std::string Volume::getUUID() {
    if (m_uuid_cached)
        return m_uuid;
    m_uuid = readRCFile("UUID");
    m_uuid_cached = true;
    return m_uuid;
}

std::string Volume::getSerial() {
    if (m_serial_cached)
        return m_serial;
    m_serial = readRCFile("SERIAL");
    m_serial_cached = true;
    return m_serial;
}

std::string Volume::getLabel() {
    if (m_label_cached)
        return m_label;
    m_label = readRCFile("LABEL");
    m_label_cached = true;
    return m_label;
}

void Volume::setLabel(std::string_view label) {
    if (writeRCFile("LABEL", label)) {
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
    if (!this) throw std::invalid_argument("Volume::resolve: null Volume");
    return std::make_unique<VolumeFile>(this, std::string(path));
}

std::string Volume::normalize(std::string_view path, bool optional) const {
    if (path.empty()) {
        if (optional) {
            return "/";
        } else {
            throw std::invalid_argument("Volume::normalize: path is mandatory but it's empty");
        }
    }
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

std::string Volume::toRealPath(std::string_view path) const {
    return normalize(path);
}

std::vector<std::unique_ptr<FileStatus>> Volume::readDir(std::string_view path, bool recursive) {
    if (path.empty()) throw std::invalid_argument("Volume::readDir: path is required");
    std::vector<std::unique_ptr<FileStatus>> list;
    readDir(list, path, recursive);
    return move(list);
}

bool Volume::createDirectories(std::string_view _path) {
    if (_path.empty()) throw std::invalid_argument("Volume::createDirectories: path is required");
    std::string path = normalize(_path);
    if (path.empty() || path == "/") {
        return false;
    }

    if (exists(path)) {
        if (!isDirectory(path))
            throw IOException("createDirectories", std::string(path),
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
        return false;

    // reverse iterate and create directories
    for (auto it = mkdir_list.rbegin(); it != mkdir_list.rend(); ++it) {
        if (!createDirectory(*it))
            return false;
    }
    return true;
}

bool Volume::createParentDirectories(std::string_view path) {
    if (path.empty()) throw std::invalid_argument("Volume::createParentDirectories: path is required");
    normalize(path);
    if (path.empty() || path == "/")
        return false;
    size_t last_slash = path.find_last_of('/');
    if (last_slash == std::string::npos)
        return false;
    std::string_view parent = path.substr(0, last_slash);
    return createDirectories(parent);
}

bool Volume::removeFile(std::string_view _path) {
    if (_path.empty()) throw std::invalid_argument("Volume::removeFile: path is required");
    std::string path = normalize(_path);
    if (!exists(path))
        return false;
    if (!isFile(path))
        throw IOException("removeFile", std::string(path), "Path is not a file");
    removeFileUnchecked(path);
    return true;
}

bool Volume::copyFile(std::string_view _src, std::string_view _dest, bool overwrite) {
    if (_src.empty()) throw std::invalid_argument("Volume::copyFile: src is empty");
    if (_dest.empty()) throw std::invalid_argument("Volume::copyFile: dest is empty");
    std::string src = normalize(_src);
    std::string dest = normalize(_dest);
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
    copyFileUnchecked(src, dest);
    return true;
}

bool Volume::moveFile(std::string_view _src, std::string_view _dest, bool overwrite) {
    if (_src.empty()) throw std::invalid_argument("Volume::moveFile: src is empty");
    if (_dest.empty()) throw std::invalid_argument("Volume::moveFile: dest is empty");
    std::string src = normalize(_src);
    std::string dest = normalize(_dest);
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
                throw IOException("moveFile", std::string(dest), "Failed to remove destination file");
        } else {
            throw IOException("moveFile", std::string(dest), "Destination file already exists");
        }
    }

    moveFileUnchecked(src, dest);
    return true;
}

bool Volume::rename(std::string_view _src, std::string_view _dest, bool overwrite) {
    if (_src.empty()) throw std::invalid_argument("Volume::rename: src is empty");
    if (_dest.empty()) throw std::invalid_argument("Volume::rename: dest is empty");
    std::string src = normalize(_src);
    std::string dest = normalize(_dest);
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
    renameFileUnchecked(src, dest);
    return true;
}

std::unique_ptr<Reader> Volume::newReader(std::string_view path, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::newReader: path is required");
    auto in = newInputStream(path);
    if (!in)
        return nullptr;
    auto data = in->readBytesUntilEOF();
    in->close();
    printf("volumn read: %d bytes => %s.", data.size(), data.data());
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

std::unique_ptr<Writer> Volume::newWriter(std::string_view path, bool append, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::newWriter: path is required");
    auto out = newOutputStream(path, append);
    if (!out)
        return nullptr;
    return std::make_unique<PrintStream>(std::move(out), encoding);
}

std::unique_ptr<RandomReader> Volume::newRandomReader(std::string_view path, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::newRandomReader: path is required");
    auto in = newRandomInputStream(path);
    if (!in)
        return nullptr;
    std::vector<uint8_t> data = in->readBytesUntilEOF();
    in->close();
    icu::UnicodeString unicode = unicode::fromEncoding(data, encoding);
    std::u32string u32 = unicode::convertToU32(unicode);
    return std::make_unique<U32stringReader>(u32);
}

std::string Volume::readFileUTF8(std::string_view path) {
    if (path.empty()) throw std::invalid_argument("Volume::readFileUTF8: path is required");
    std::vector<uint8_t> data = readFile(path);
    return std::string(data.begin(), data.end());
}

std::string Volume::readFileString(std::string_view path, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::readFileString: path is required");
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

std::deque<std::string> Volume::readLines(std::string_view path, int maxLines, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::readLines: path is required");
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

std::deque<std::string> Volume::readLastLines(std::string_view path, int maxLines, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::readLastLines: path is required");
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

void Volume::writeFileUTF8(std::string_view path, std::string_view data) {
    if (path.empty()) throw std::invalid_argument("Volume::writeFileUTF8: path is required");
    std::vector<uint8_t> bytes(data.begin(), data.end());
    writeFile(path, bytes);
}

void Volume::writeFileString(std::string_view path, std::string_view data, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::writeFileString: path is required");
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

void Volume::writeLines(std::string_view path, const std::vector<std::string>& lines, std::string_view encoding) {
    if (path.empty()) throw std::invalid_argument("Volume::writeLines: path is required");
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
