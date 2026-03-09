#include "LocalVolume.hpp"

#include "DirEntry.hpp"
#include "FileStatus.hpp"

#include "../io/IOException.hpp"
#include "../io/InputStreamReader.hpp"
#include "../io/LocalInputStream.hpp"
#include "../io/LocalOutputStream.hpp"
#include "../io/OutputStreamWriter.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

#include <sys/stat.h>

#include <dirent.h>
#include <unistd.h>

namespace fs = std::filesystem;

static void strip_trailing_slashes(std::string& s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
}

LocalVolume::LocalVolume(std::string_view rootPath) : m_rootPath(rootPath) {
    strip_trailing_slashes(m_rootPath);
    if (!fs::exists(m_rootPath)) {
        fs::create_directories(m_rootPath);
    }
}

void LocalVolume::setRootPath(std::string_view rootPath) {
    m_rootPath = rootPath;
    strip_trailing_slashes(m_rootPath);
}

VolumeType LocalVolume::getType() const {
    cacheMountInfo();
    return m_type;
}

std::string LocalVolume::getDefaultLabel() const {
    return "Local Volume";
}

std::string LocalVolume::getLabel() {
    // Check if root path is a mount point and use device label
    cacheMountInfo();
    if (!m_cachedLabel.empty()) {
        return m_cachedLabel;
    }
    return Volume::getLabel();
}

void LocalVolume::setLabel(std::string_view label) {
    Volume::setLabel(label);
}

std::string LocalVolume::getUUID() {
    // Check if root path is a mount point and use device UUID
    cacheMountInfo();
    return m_cachedUUID;
}

std::string LocalVolume::getSerial() {
    // For now, return empty string or could use device serial number
    // This could be extended to get device serial from /sys/block/ or similar
    return getUUID();
}

const std::optional<std::string>& LocalVolume::getMountPoint() const {
    cacheMountInfo();
    return m_mountPoint;
}

const std::optional<std::string>& LocalVolume::getDevice() const {
    cacheMountInfo();
    return m_device;
}

std::string LocalVolume::resolveLocal(std::string_view _path) const {
    std::string path = normalize(_path);
    if (path.empty()) {
        return ""; // keep unspecified semantic
    }
    std::string result;
    if (path.front() == '/') {
        result = m_rootPath + path;
    } else {
        result = m_rootPath + "/" + path;
    }
    // No trailing slash: libstdc++ can segfault in path::_M_split_cmpts for "base/"
    if (result.size() > 1 && result.back() == '/') result.pop_back();
    return result;
}

bool LocalVolume::exists(std::string_view path) const {
    std::string localPath = resolveLocal(path);
    return fs::exists(localPath);
}

bool LocalVolume::isFile(std::string_view _path) const {
    std::string localPath = resolveLocal(_path);
    return fs::is_regular_file(localPath);
}

bool LocalVolume::isDirectory(std::string_view _path) const {
    std::string localPath = resolveLocal(_path);
    return fs::is_directory(localPath);
}

bool LocalVolume::stat(std::string_view _path, FileStatus* st) const {
    std::string localPath = resolveLocal(_path);
    
    auto fs_status = fs::status(localPath);
    if (fs::is_directory(fs_status))
        st->type |= FileType::DIRECTORY;
    else {
        st->type |= FileType::REGULAR_FILE;
        std::error_code ec;
        st->size = fs::file_size(localPath, ec);
        if (ec) st->size = 0;
    }
    
    auto ftime = fs::last_write_time(localPath);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    st->modifiedTime = std::chrono::system_clock::to_time_t(sctp);
    st->accessTime = st->modifiedTime; // Simplified
    st->createTime = st->modifiedTime; // Simplified
    
    return true;
}

// Add one directory's entries to list using POSIX opendir/readdir (avoids libstdc++ path segfault).
static void readDirOne(const std::string& dirPath, std::vector<std::unique_ptr<FileStatus>>& list) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) {
        throw IOException("readDir", dirPath, std::strerror(errno));
    }
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;
        std::string fullPath = dirPath;
        if (fullPath.back() != '/') fullPath += '/';
        fullPath += ent->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        auto fileStatus = std::make_unique<FileStatus>();
        fileStatus->name = ent->d_name;
        fileStatus->type = S_ISDIR(st.st_mode) ? FileType::DIRECTORY : FileType::REGULAR_FILE;
        fileStatus->size = S_ISREG(st.st_mode) ? static_cast<uint64_t>(st.st_size) : 0;
        fileStatus->modifiedTime = st.st_mtime;
        fileStatus->accessTime = st.st_atime;
        fileStatus->createTime = st.st_ctime;
        list.push_back(std::move(fileStatus));
    }
    closedir(dir);
}

// Recursively collect entries under dirPath (relative names for subdirs).
static void readDirRecursive(const std::string& dirPath, const std::string& prefix,
                             std::vector<std::unique_ptr<FileStatus>>& list) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
            continue;
        std::string fullPath = dirPath;
        if (fullPath.back() != '/') fullPath += '/';
        fullPath += ent->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;

        std::string displayName = prefix.empty() ? ent->d_name : prefix + "/" + ent->d_name;
        auto fileStatus = std::make_unique<FileStatus>();
        fileStatus->name = displayName;
        fileStatus->type = S_ISDIR(st.st_mode) ? FileType::DIRECTORY : FileType::REGULAR_FILE;
        fileStatus->size = S_ISREG(st.st_mode) ? static_cast<uint64_t>(st.st_size) : 0;
        fileStatus->modifiedTime = st.st_mtime;
        fileStatus->accessTime = st.st_atime;
        fileStatus->createTime = st.st_ctime;
        list.push_back(std::move(fileStatus));
        if (S_ISDIR(st.st_mode)) {
            readDirRecursive(fullPath, displayName, list);
        }
    }
    closedir(dir);
}

void LocalVolume::readDir(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view _path, bool recursive) {
    std::string dirPath = resolveLocal(_path);
    
    struct stat sb;
    if (::stat(dirPath.c_str(), &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        throw IOException("readDir", std::string(_path), "Path is not a directory");
    }

    try {
        if (recursive) {
            readDirRecursive(dirPath, "", list);
        } else {
            readDirOne(dirPath, list);
        }
    } catch (const IOException&) {
        throw;
    } catch (const std::exception& e) {
        throw IOException("readDir", std::string(_path), e.what());
    }
}

bool LocalVolume::createDirectory(std::string_view _path) {
    std::string localPath = resolveLocal(_path);
    if (fs::is_directory(localPath))
        return false;
    if (fs::exists(localPath)) {
        throw IOException("mkdir", std::string(_path), "File with same name exists");
    }
    
    std::error_code ec;
    if (!fs::create_directory(localPath, ec)) {
        if (ec) {
            throw IOException("mkdir", std::string(_path), ec.message());
        }
    }
    return true;
}

bool LocalVolume::removeDirectory(std::string_view _path) {
    std::string localPath = resolveLocal(_path);
    
    if (!fs::exists(localPath)) {
        return false;
    }
    if (!fs::is_directory(localPath)) {
        throw IOException("rmdir", std::string(_path), "Path is not a directory");
    }

    std::error_code ec;
    if (!fs::remove(localPath, ec)) {
        if (ec) {
            throw IOException("rmdir", std::string(_path), ec.message());
        } else {
            throw IOException("rmdir", std::string(_path), "Directory not empty or does not exist");
        }
    }
    return true;
}

// std::unique_ptr<IReadStream> LocalVolume::openForRead(std::string_view _path, std::string_view /*encoding*/) {
//     std::string localPath = resolveLocal(_path);
//     if (!fs::exists(localPath))
//         throw IOException("openForRead", std::string(_path), "Path does not exist");
//     if (!fs::is_regular_file(localPath))
//         throw IOException("openForRead", std::string(_path), "Path is not a regular file");
//     auto stream = std::make_unique<LocalInputStream>(localPath);
//     return stream->isOpen() ? std::move(stream) : nullptr;
// }

// std::unique_ptr<IWriteStream> LocalVolume::openForWrite(std::string_view _path, bool append, std::string_view /*encoding*/) {
//     std::string localPath = resolveLocal(_path);
//     if (fs::exists(localPath) && !fs::is_regular_file(localPath))
//         throw IOException("openForWrite", std::string(_path), "Path is not a regular file");
//     auto raw = std::make_unique<LocalOutputStream>(localPath, append);
//     if (!raw->isOpen())
//         return nullptr;
//     return std::make_unique<EncodingWriteStreamToOutputStream>(std::move(raw), "UTF-8");
// }

std::unique_ptr<InputStream> LocalVolume::newInputStream(std::string_view _path) {
    std::string localPath = resolveLocal(_path);
    if (!fs::exists(localPath))
        throw IOException("newInputStream", std::string(_path), "Path does not exist");
    if (!fs::is_regular_file(localPath))
        throw IOException("newInputStream", std::string(_path), "Path is not a regular file");
    auto stream = std::make_unique<LocalInputStream>(localPath);
    return stream->isOpen() ? std::move(stream) : nullptr;
}

std::unique_ptr<OutputStream> LocalVolume::newOutputStream(std::string_view _path, bool append) {
    std::string localPath = resolveLocal(_path);
    if (fs::exists(localPath) && !fs::is_regular_file(localPath))
        throw IOException("newOutputStream", std::string(_path), "Path is not a regular file");
    auto stream = std::make_unique<LocalOutputStream>(localPath, append);
    return stream->isOpen() ? std::move(stream) : nullptr;
}

std::unique_ptr<Reader> LocalVolume::newReader(std::string_view _path, std::string_view encoding) {
    std::string localPath = resolveLocal(_path);
    if (!fs::exists(localPath))
        throw IOException("newReader", std::string(_path), "Path does not exist");
    if (!fs::is_regular_file(localPath))
        throw IOException("newReader", std::string(_path), "Path is not a regular file");
    auto stream = std::make_unique<LocalInputStream>(localPath);
    auto reader = std::make_unique<InputStreamReader>(std::move(stream), encoding);
    return reader;
}

std::unique_ptr<RandomInputStream> LocalVolume::newRandomInputStream(std::string_view _path) {
    std::string localPath = resolveLocal(_path);
    if (!fs::exists(localPath))
        throw IOException("newRandomInputStream", std::string(_path), "Path does not exist");
    if (!fs::is_regular_file(localPath))
        throw IOException("newRandomInputStream", std::string(_path), "Path is not a regular file");
    auto stream = std::make_unique<LocalInputStream>(localPath);
    return stream->isOpen() ? std::move(stream) : nullptr;
}

std::unique_ptr<RandomReader> LocalVolume::newRandomReader(std::string_view _path, std::string_view encoding) {
    return Volume::newRandomReader(_path, encoding);
}

std::unique_ptr<Writer> LocalVolume::newWriter(std::string_view _path, bool append, std::string_view encoding) {
    std::string localPath = resolveLocal(_path);
    if (fs::exists(localPath) && !fs::is_regular_file(localPath))
        throw IOException("newWriter", std::string(_path), "Path is not a regular file");
    auto stream = std::make_unique<LocalOutputStream>(localPath, append);
    auto writer = std::make_unique<OutputStreamWriter>(std::move(stream), encoding);
    return writer;
}

std::vector<uint8_t> LocalVolume::readFile(std::string_view _path) {
    std::string localPath = resolveLocal(_path);
    if (!fs::exists(localPath))
        throw IOException("readFile", std::string(_path), "Path does not exist");
    if (!fs::is_regular_file(localPath))
        throw IOException("readFile", std::string(_path), "Path is not a regular file");
    
    std::ifstream file(localPath, std::ios::binary);
    if (!file) return {};
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return data;
}

void LocalVolume::writeFile(std::string_view _path, const std::vector<uint8_t>& data) {
    std::string localPath = resolveLocal(_path);
    if (!fs::exists(localPath))
        throw IOException("writeFile", std::string(_path), "Path does not exist");
    if (!fs::is_regular_file(localPath))
        throw IOException("writeFile", std::string(_path), "Path is not a regular file");
    
    std::ofstream file(localPath, std::ios::binary);
    if (!file.is_open()) {
        throw IOException("writeFile", std::string(_path), "Failed to open file for writing");
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!file.good()) {
        throw IOException("writeFile", std::string(_path), "Error writing file");
    }
}

void LocalVolume::removeFileUnchecked(std::string_view path) {
    std::string localPath = resolveLocal(path);
    std::error_code ec;
    if (!fs::remove(localPath, ec)) {
        throw IOException("removeFile", std::string(path), ec.message());
    }
}

void LocalVolume::copyFileUnchecked(std::string_view src, std::string_view dest) {
    std::string localSrc = resolveLocal(src);
    std::string localDest = resolveLocal(dest);
    std::error_code ec;
    fs::copy_file(localSrc, localDest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        throw IOException("copyFile", std::string(src), ec.message());
    }
}

void LocalVolume::moveFileUnchecked(std::string_view src, std::string_view dest) {
    std::string localSrc = resolveLocal(src);
    std::string localDest = resolveLocal(dest);
    std::error_code ec;
    try {
        fs::rename(localSrc, localDest);
    } catch (const fs::filesystem_error& e) {
        // cross volume rename is not supported, so we need to copy and remove the source file
        ec = e.code();
        try {
            fs::copy(localSrc, localDest, fs::copy_options::overwrite_existing);
            fs::remove(localSrc);
        } catch (const fs::filesystem_error& e) {
            ec = e.code();
        }
    }

    if (ec) {
        throw IOException("moveFile", std::string(src), ec.message());
    }
}

void LocalVolume::renameFileUnchecked(std::string_view src, std::string_view dest) {
    std::string localSrc = resolveLocal(src);
    std::string localDest = resolveLocal(dest);
    std::error_code ec;
    fs::rename(localSrc, localDest, ec);
    if (ec) {
        throw IOException("renameFile", std::string(src), ec.message());
    }
}

std::string LocalVolume::getTempDir() {
    return fs::temp_directory_path().string();
}

std::string LocalVolume::createTempFile(std::string_view prefix, std::string_view suffix) {
    std::string tempDir = getTempDir();
    std::string tempFile = tempDir + "/" + std::string(prefix) + std::to_string(std::time(nullptr)) + std::string(suffix);
    
    // Create empty file
    std::ofstream file(tempFile);
    file.close();
    
    return tempFile;
}

// 

#if defined(__linux__)

bool LocalVolume::isMountPoint(const std::string& localPath) const {
    struct ::stat pathStat, parentStat;
    
    if (::stat(localPath.c_str(), &pathStat) != 0) {
        return false;
    }
    
    // Get parent directory
    std::string parent = fs::path(localPath).parent_path().string();
    if (parent.empty() || parent == "/") {
        // Root directory is always a mount point
        return true;
    }
    
    if (::stat(parent.c_str(), &parentStat) != 0) {
        return false;
    }
    
    // If device IDs differ, it's a mount point
    return pathStat.st_dev != parentStat.st_dev;
}

std::string LocalVolume::getMountDevice(const std::string& mountPoint) const {
    std::string canonicalMount;
    try {
        canonicalMount = fs::canonical(mountPoint).string();
    } catch (...) {
        canonicalMount = fs::absolute(mountPoint).string();
    }
    
    // Try /proc/self/mountinfo first (more detailed)
    std::ifstream mountinfo("/proc/self/mountinfo");
    if (mountinfo.is_open()) {
        std::string line;
        while (std::getline(mountinfo, line)) {
            std::istringstream iss(line);
            std::vector<std::string> tokens;
            std::string token;
            
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            // /proc/self/mountinfo format:
            // ID parent-ID major:minor root mount-point [options] - type device [options]
            // Example: 36 35 98:0 /mnt/point /mnt/point rw,noatime - ext4 /dev/sda1 rw
            if (tokens.size() >= 10) {
                // Find the "-" separator
                size_t dashIdx = 0;
                for (size_t i = 0; i < tokens.size(); ++i) {
                    if (tokens[i] == "-") {
                        dashIdx = i;
                        break;
                    }
                }
                
                if (dashIdx > 0 && dashIdx + 2 < tokens.size()) {
                    std::string mountPath = tokens[4];
                    std::string device = tokens[dashIdx + 2];
                    
                    std::string canonicalMountPath;
                    try {
                        canonicalMountPath = fs::canonical(mountPath).string();
                    } catch (...) {
                        canonicalMountPath = fs::absolute(mountPath).string();
                    }
                    
                    if (canonicalMountPath == canonicalMount) {
                        return device;
                    }
                }
            }
        }
    }
    
    // Fallback to /proc/mounts
    std::ifstream mounts("/proc/mounts");
    if (mounts.is_open()) {
        std::string line;
        while (std::getline(mounts, line)) {
            std::istringstream iss(line);
            std::vector<std::string> tokens;
            std::string token;
            
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            // /proc/mounts format: device mount-point type options
            if (tokens.size() >= 2) {
                std::string device = tokens[0];
                std::string mountPath = tokens[1];
                
                std::string canonicalMountPath;
                try {
                    canonicalMountPath = fs::canonical(mountPath).string();
                } catch (...) {
                    canonicalMountPath = fs::absolute(mountPath).string();
                }
                
                if (canonicalMountPath == canonicalMount) {
                    return device;
                }
            }
        }
    }
    
    return "";
}

std::string LocalVolume::getFilesystemUUID(const std::string& device) const {
    if (device.empty()) {
        return "";
    }
    
    // Try to get UUID using blkid command
    std::string command = "blkid -s UUID -o value " + device + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string uuid(buffer);
            // Remove trailing newline
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\n'), uuid.end());
            uuid.erase(std::remove(uuid.begin(), uuid.end(), '\r'), uuid.end());
            pclose(pipe);
            if (!uuid.empty()) {
                return uuid;
            }
        }
        pclose(pipe);
    }
    
    // Fallback: try to find UUID in /dev/disk/by-uuid/
    try {
        for (const auto& entry : fs::directory_iterator("/dev/disk/by-uuid")) {
            if (fs::is_symlink(entry.path())) {
                try {
                    fs::path symlinkPath = entry.path();
                    fs::path target = fs::read_symlink(symlinkPath);
                    fs::path canonicalTarget = fs::canonical(symlinkPath);
                    fs::path canonicalDevice = fs::canonical(device);
                    
                    if (canonicalTarget == canonicalDevice) {
                        return entry.path().filename().string();
                    }
                } catch (...) {
                    // Ignore errors for individual entries
                    continue;
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return "";
}

std::string LocalVolume::getFilesystemLabel(const std::string& device) const {
    if (device.empty()) {
        return "";
    }
    
    // Try to get label using blkid command
    std::string command = "blkid -s LABEL -o value " + device + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string label(buffer);
            // Remove trailing newline
            label.erase(std::remove(label.begin(), label.end(), '\n'), label.end());
            label.erase(std::remove(label.begin(), label.end(), '\r'), label.end());
            pclose(pipe);
            if (!label.empty()) {
                return label;
            }
        }
        pclose(pipe);
    }
    
    // Fallback: try to find label in /dev/disk/by-label/
    try {
        for (const auto& entry : fs::directory_iterator("/dev/disk/by-label")) {
            if (fs::is_symlink(entry.path())) {
                try {
                    fs::path symlinkPath = entry.path();
                    fs::path target = fs::read_symlink(symlinkPath);
                    fs::path canonicalTarget = fs::canonical(symlinkPath);
                    fs::path canonicalDevice = fs::canonical(device);
                    
                    if (canonicalTarget == canonicalDevice) {
                        return entry.path().filename().string();
                    }
                } catch (...) {
                    // Ignore errors for individual entries
                    continue;
                }
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return "";
}

void LocalVolume::cacheMountInfo() const {
    if (m_mountInfoCached) {
        return;
    }
    
    if (isMountPoint(m_rootPath)) {
        std::string device = getMountDevice(m_rootPath);
        if (!device.empty()) {
            m_device = device;

            try {
                std::string canonicalMount = fs::canonical(m_rootPath).string();
                m_mountPoint = canonicalMount;
            } catch (...) {
                m_mountPoint = m_rootPath;
            }

            // Classify volume type based on device string
            if (false) {
            } else if (m_rootPath.find("/dev/") == 0
                    || m_rootPath.find("/proc/") == 0
                    || m_rootPath.find("/run/") == 0
                    || m_rootPath.find("/sys/") == 0
                    || m_rootPath.find("/tmp/") == 0
                    ) {
                m_type = VolumeType::SYSTEM;
            } else if (device.rfind("/dev/sr", 0) == 0 
                    || device.rfind("/dev/cd", 0) == 0
                    ) {
                m_type = VolumeType::CDROM;
            } else if (!device.empty() && device[0] == '/' 
                    && device.find("/dev/") == 0) {
                m_type = VolumeType::HARDDISK;
            } else if (device.rfind("//", 0) == 0) {
                m_type = VolumeType::NETWORK;
            } else if (device.find(':') != std::string::npos && device[0] != '/') {
                m_type = VolumeType::NETWORK;
            } else {
                m_type = VolumeType::OTHER;
            }

            m_cachedUUID = getFilesystemUUID(device);
            m_cachedLabel = getFilesystemLabel(device);
        }
    }

    m_mountInfoCached = true;
}

#elif defined(WINDOWS)


#endif