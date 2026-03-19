#include "VolumeFile.hpp"

#include "Volume.hpp"

#include "../io/IOException.hpp"
#include "../util/unicode.hpp"

extern "C" {
#include <bas/proc/env.h>
}

#include <unistd.h>
#include <limits.h>

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

VolumeFile::VolumeFile()
    : m_volume(nullptr)
{}

VolumeFile::VolumeFile(Volume* volume, std::string path)
    : m_volume(volume)
    , m_path(path)
{}

void VolumeFile::setVolume(Volume* volume) {
    m_volume = volume;
}

void VolumeFile::setPath(std::string path) {
    m_path = path;
}

std::string VolumeFile::getLocalFile() const {
    if (!m_volume || m_path.empty()) {
        return "";
    }
    // For LocalVolume, this might return the actual local path
    // For SeczureVolume, this will return empty
    // This is volume-specific behavior
    std::string localFile = m_volume->getLocalFile(m_path);
    return localFile;
}

size_t VolumeFile::readFile(uint8_t* buf, size_t off, size_t len, int64_t file_offset, std::ios::seekdir seek_dir) const {
    if (!m_volume || m_path.empty()) {
        return 0;
    }
    auto stream = newRandomInputStream();
    if (!stream) {
        return 0;
    }
    if (!stream->seek(file_offset, seek_dir)) {
        return 0;
    }
    return stream->read(buf, off, len);
}

size_t VolumeFile::writeFile(const uint8_t* buf, size_t off, size_t len, bool append) const {
    if (!m_volume || m_path.empty()) {
        return 0;
    }
    auto stream = m_volume->newOutputStream(m_path, append);
    if (!stream) {
        return 0;
    }
    return stream->write(buf, off, len);
}

std::vector<uint8_t> VolumeFile::readFile() const {
    if (!m_volume) {
        return std::vector<uint8_t>();
    }
    if (m_path.empty()) {
        return std::vector<uint8_t>();
    }
    return m_volume->readFile(m_path);
}

std::string VolumeFile::readFileString(std::string_view encoding) const {
    if (!m_volume || m_path.empty()) {
        return "";
    }
    
    std::vector<uint8_t> data = m_volume->readFile(m_path);
    if (data.empty()) {
        return "";
    }
    
    // For UTF-8 encoding (default), convert directly
    if (encoding == "UTF-8" || encoding.empty()) {
        return std::string(data.begin(), data.end());
    }
    
    // Convert from specified encoding to UTF-8 using boost::locale
    auto unicode = unicode::fromEncoding(data, encoding);
    auto utf8 = unicode::convertToUTF8(unicode);
    return utf8;
}

std::vector<std::string> VolumeFile::readFileLines(std::string_view encoding) const {
    std::string data = readFileString(encoding);
    std::vector<std::string> lines;
    std::istringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

void VolumeFile::writeFile(const std::vector<uint8_t>& data) const {
    if (!m_volume || m_path.empty()) {
        throw IOException("writeFile", m_path, "Volume or path is null/empty");
    }
    m_volume->writeFile(m_path, data);
}

void VolumeFile::writeFileString(std::string_view data, std::string_view encoding) const {
    if (!m_volume || m_path.empty()) {
        throw IOException("writeFile", m_path, "Volume or path is null/empty");
    }
    
    // Convert string to bytes based on encoding
    std::vector<uint8_t> bytes;
    if (encoding == "UTF-8" || encoding.empty()) {
        bytes.assign(data.begin(), data.end());
    } else {
        // Convert from specified encoding to UTF-8
        auto unicode = unicode::fromEncoding(data, encoding);
        auto utf8 = unicode::convertToUTF8(unicode);
        bytes.assign(utf8.begin(), utf8.end());
    }
    
    m_volume->writeFile(m_path, bytes);
}

void VolumeFile::writeFileLines(const std::vector<std::string>& lines, std::string_view encoding) const {
    // join lines with newline
    std::string content;
    for (const auto& line : lines) {
        content += line + "\n";
    }
    writeFileString(content, encoding);
}

std::unique_ptr<VolumeFile> VolumeFile::getParentFile() const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    int lastSlash = m_path.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return nullptr;
    }
    std::string parentPath = m_path.substr(0, lastSlash);
    if (parentPath.empty()) {
        return getRootFile();
    }
    return std::make_unique<VolumeFile>(m_volume, parentPath);
}

std::unique_ptr<VolumeFile> VolumeFile::getRootFile() const {
    return m_volume->getRootFile();
}

std::unique_ptr<VolumeFile> VolumeFile::resolve(std::string_view relativePath) const {
    if (!m_volume) {
        throw IOException("VolumeFile::resolve: null volume");
    }
    
    if (relativePath.empty()) {
        // Return a copy of this VolumeFile
        return std::make_unique<VolumeFile>(m_volume, m_path);
    }
    
    // Determine base path for resolution
    std::string basePath = m_path;
    
    // If current path is a file, resolve relative to its parent directory
    if (!basePath.empty() && m_volume->isFile(basePath)) {
        // Get parent directory
        fs::path pathObj(basePath);
        basePath = pathObj.parent_path().string();
        if (basePath.empty() || basePath == ".") {
            basePath = "/";
        }
    }
    
    // If basePath is empty, use root
    if (basePath.empty()) {
        basePath = "/";
    }
    
    // Handle absolute paths
    if (relativePath[0] == '/') {
        // Absolute path - use as-is (but ensure it's normalized)
        fs::path resolved(relativePath);
        resolved = resolved.lexically_normal();
        return std::make_unique<VolumeFile>(m_volume, resolved.string());
    }
    
    // Relative path - resolve it
    fs::path basePathObj(basePath);
    fs::path relativePathObj(relativePath);
    fs::path resolved = basePathObj / relativePathObj;
    resolved = resolved.lexically_normal();
    
    // Ensure it starts with "/" for virtual filesystem paths
    std::string resolvedStr = resolved.string();
    if (resolvedStr[0] != '/') {
        resolvedStr = "/" + resolvedStr;
    }
    
    return std::make_unique<VolumeFile>(m_volume, resolvedStr);
}

bool VolumeFile::exists() const {
    if (!m_volume || m_path.empty()) {
        return false;
    }
    return m_volume->exists(m_path);
}

bool VolumeFile::isFile() const {
    if (!m_volume || m_path.empty()) {
        return false;
    }
    return m_volume->isFile(m_path);
}

bool VolumeFile::isDirectory() const {
    if (!m_volume || m_path.empty()) {
        return false;
    }
    return m_volume->isDirectory(m_path);
}

bool VolumeFile::canRead() const {
    // Check if file exists and volume supports reading
    if (!m_volume || m_path.empty()) {
        return false;
    }
    // For now, assume if file exists, it can be read
    // TODO: Implement proper permission checking if Volume has canRead method
    return exists();
}

bool VolumeFile::canWrite() const {
    // Check if file exists and volume supports writing
    if (!m_volume || m_path.empty()) {
        return false;
    }
    // For now, assume if file exists, it can be written
    // TODO: Implement proper permission checking if Volume has canWrite method
    return exists();
}

bool VolumeFile::createDirectory() const {
    return m_volume->createDirectory(m_path);
}

bool VolumeFile::createDirectories() const {
    return m_volume->createDirectories(m_path);
}

bool VolumeFile::createParentDirectories() const {
    return m_volume->createParentDirectories(m_path);
}

bool VolumeFile::removeDirectory() const {
    return m_volume->removeDirectory(m_path);
}

bool VolumeFile::removeFile() const {
    return m_volume->removeFile(m_path);
}

bool VolumeFile::renameTo(std::string_view destPath, bool overwrite) const {
    if (destPath.empty()) throw std::invalid_argument("VolumeFile::renameTo: destPath is required");
    return m_volume->rename(m_path, destPath, overwrite);
}

bool VolumeFile::copyTo(std::string_view destPath, bool overwrite) const {
    if (destPath.empty()) throw std::invalid_argument("VolumeFile::copyTo: destPath is required");
    return m_volume->copyFile(m_path, destPath, overwrite);
}

bool VolumeFile::moveTo(std::string_view destPath, bool overwrite) const {
    if (destPath.empty()) throw std::invalid_argument("VolumeFile::moveTo: destPath is required");
    return m_volume->moveFile(m_path, destPath, overwrite);
}

void VolumeFile::readDir(std::vector<std::unique_ptr<FileStatus>>& list, bool recursive) const {
    if (!m_volume || m_path.empty()) {
        throw IOException("readDir", m_path, "Volume or path is null/empty");
    }
    m_volume->readDir_inplace(list, m_path, recursive);
}

bool VolumeFile::stat(FileStatus* status) const {
    if (!m_volume || m_path.empty()) {
        throw IOException("stat", m_path, "Volume or path is null/empty");
    }
    if (!status) throw std::invalid_argument("VolumeFile::stat: null status");
    return m_volume->stat(m_path, status);
}

std::string VolumeFile::exportToTempFile() const {
    if (!m_volume || m_path.empty()) {
        return "";
    }
    
    // First, try to get local file (works for LocalVolume)
    std::string localFile = getLocalFile();
    if (!localFile.empty()) {
        return localFile;
    }

    fs::path pathInfo(m_path);
    std::string baseName = pathInfo.filename().string();
    if (baseName.empty()) {
        baseName = "file";
    }
    std::string ext = pathInfo.extension().string();
    if (!ext.empty()) {
        ext = "." + ext;
    }
    
    char tmpname[PATH_MAX];
    std::string templ = baseName + "_XXXXXX" + ext;

    FILE *out = emktemp_open(tmpname, sizeof(tmpname), 
        templ.c_str(), NULL, NULL);
    if (!out) {
        return "";
    }

    // Read file from virtual filesystem
    std::vector<uint8_t> data = readFile();
    if (data.empty()) {
        return "";
    }
    
    // Write to temporary file
    size_t cb = fwrite(data.data(), 1, data.size(), out);
    fclose(out);
    
    if (cb != data.size()) {
        fs::remove(tmpname);
        return "";
    }
    
    return std::string(tmpname);
}

bool VolumeFile::requiresTempFileForPlayback(std::string_view fileName) const {
    if (fileName.empty()) {
        return true;  // Be conservative, use temp file
    }
    
    fs::path filePath(fileName);
    std::string ext = filePath.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Formats that typically don't support streaming and require temp file
    // These formats often have backends that don't support streaming
    if (ext == "wmv" || ext == "asf" || ext == "rm" || ext == "rmvb") {
        return true;
    }
    
    return false;
}

// std::unique_ptr<IReadStream> VolumeFile::openForRead() const {
//     if (!m_volume || m_path.empty()) {
//         return nullptr;
//     }
//     return m_volume->openForRead(m_path);
// }

// std::unique_ptr<IWriteStream> VolumeFile::openForWrite(bool append) const {
//     if (!m_volume || m_path.empty()) {
//         return nullptr;
//     }
//     return m_volume->openForWrite(m_path, append);
// }

std::unique_ptr<InputStream> VolumeFile::newInputStream() const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newInputStream(m_path);
}

std::unique_ptr<RandomInputStream> VolumeFile::newRandomInputStream() const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newRandomInputStream(m_path);
}

std::unique_ptr<OutputStream> VolumeFile::newOutputStream(bool append) const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newOutputStream(m_path, append);
}

std::unique_ptr<Reader> VolumeFile::newReader(std::string_view encoding) const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newReader(m_path, encoding);
}

std::unique_ptr<RandomReader> VolumeFile::newRandomReader(std::string_view encoding) const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newRandomReader(m_path, encoding);
}

std::unique_ptr<Writer> VolumeFile::newWriter(bool append, std::string_view encoding) const {
    if (!m_volume || m_path.empty()) {
        return nullptr;
    }
    return m_volume->newWriter(m_path, append, encoding);
}
