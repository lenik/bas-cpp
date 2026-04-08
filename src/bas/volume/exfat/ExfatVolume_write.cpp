#include "ExfatVolume.hpp"
#include "../../io/IOException.hpp"

#include <algorithm>
#include <cstring>
#include <chrono>

// Helper to get current time as exFAT timestamp
static uint32_t getExfatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);
    
    // exFAT timestamp format
    uint32_t timestamp = 0;
    timestamp |= (tm->tm_sec / 10) & 0x1F;  // 10ms count (bits 0-4)
    timestamp |= (tm->tm_min & 0x3F) << 5;   // Minutes (bits 5-10)
    timestamp |= (tm->tm_hour & 0x1F) << 11; // Hours (bits 11-15)
    timestamp |= (tm->tm_mday & 0x1F) << 16; // Day (bits 16-20)
    timestamp |= ((tm->tm_mon + 1) & 0x0F) << 21; // Month (bits 21-24)
    timestamp |= ((tm->tm_year + 1900 - 1980) & 0x7F) << 25; // Year (bits 25-31)
    
    return timestamp;
}

void ExfatVolume::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("writeFile", std::string(path), "Cannot write to root directory");
    }
    
    // Check if parent directory exists
    const std::string parent = getParentPath(normalized);
    if (!ensurePathIndexed(parent)) {
        throw IOException("writeFile", std::string(path), "Parent directory does not exist");
    }
    
    // Allocate clusters for data
    uint32_t firstCluster = 0;
    uint32_t prevCluster = 0;
    
    if (!data.empty()) {
        const uint32_t numClusters = (data.size() + m_clusterSize - 1) / m_clusterSize;
        
        for (uint32_t i = 0; i < numClusters; ++i) {
            uint32_t cluster = allocateCluster(prevCluster);
            if (cluster == 0) {
                if (firstCluster != 0) {
                    freeClusterChain(firstCluster);
                }
                throw IOException("writeFile", std::string(path), "No space left on device");
            }
            
            if (firstCluster == 0) {
                firstCluster = cluster;
            }
            
            // Write data to cluster
            const size_t offset = i * m_clusterSize;
            const size_t len = std::min<size_t>(m_clusterSize, data.size() - offset);
            const uint64_t clusterOffset = clusterToOffset(cluster);
            
            std::vector<uint8_t> clusterData(m_clusterSize, 0);
            memcpy(clusterData.data(), data.data() + offset, len);
            
            if (!writeAt(clusterOffset, clusterData.data(), clusterData.size())) {
                freeClusterChain(firstCluster);
                throw IOException("writeFile", std::string(path), "Failed to write cluster");
            }
            
            prevCluster = cluster;
        }
    }
    
    // Create or update directory entry
    createFileEntry(normalized, firstCluster, data.size());
    
    // Invalidate cache
    invalidateCache(normalized);
}

void ExfatVolume::createDirectoryThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("createDirectory", std::string(path), "Cannot create root directory");
    }
    
    // Check if parent directory exists
    const std::string parent = getParentPath(normalized);
    if (!ensurePathIndexed(parent)) {
        throw IOException("createDirectory", std::string(path), "Parent directory does not exist");
    }
    
    // Check if already exists
    if (ensurePathIndexed(normalized)) {
        throw IOException("createDirectory", std::string(path), "Path already exists");
    }
    
    // Allocate a cluster for the new directory
    uint32_t firstCluster = allocateCluster(0);
    if (firstCluster == 0) {
        throw IOException("createDirectory", std::string(path), "No space left on device");
    }
    
    // Initialize directory cluster with end-of-chain marker
    std::vector<uint8_t> emptyCluster(m_clusterSize, 0);
    writeAt(clusterToOffset(firstCluster), emptyCluster.data(), emptyCluster.size());
    
    // Create directory entry
    createDirectoryEntry(normalized, firstCluster);
    
    // Invalidate cache
    invalidateCache(normalized);
}

void ExfatVolume::removeFileThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    
    // Check if file exists
    if (!ensurePathIndexed(normalized)) {
        throw IOException("removeFile", std::string(path), "File does not exist");
    }
    
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end() || it->second.isDirectory) {
        throw IOException("removeFile", std::string(path), "File does not exist or is a directory");
    }
    
    // Free cluster chain
    freeClusterChain(it->second.firstCluster);
    
    // Delete directory entry
    deleteFileEntry(normalized);
    
    // Invalidate cache
    invalidateCache(normalized);
}

void ExfatVolume::removeDirectoryThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("removeDirectory", std::string(path), "Cannot remove root directory");
    }
    
    // Check if directory exists
    if (!ensurePathIndexed(normalized)) {
        throw IOException("removeDirectory", std::string(path), "Directory does not exist");
    }
    
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end() || !it->second.isDirectory) {
        throw IOException("removeDirectory", std::string(path), "Directory does not exist");
    }
    
    // Check if directory is empty
    const std::string prefix = normalized + "/";
    for (const auto& [fullPath, dirent] : m_dirents) {
        if (fullPath != normalized && fullPath.rfind(prefix, 0) == 0) {
            throw IOException("removeDirectory", std::string(path), "Directory is not empty");
        }
    }
    
    // Free cluster chain
    freeClusterChain(it->second.firstCluster);
    
    // Delete directory entry
    deleteDirectoryEntry(normalized);
    
    // Invalidate cache
    invalidateCache(normalized);
}

void ExfatVolume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    const std::string srcNormalized = normalizeArg(src);
    const std::string destNormalized = normalizeArg(dest);
    
    // Check source file
    if (!ensurePathIndexed(srcNormalized)) {
        throw IOException("copyFile", std::string(src), "Source file does not exist");
    }
    
    auto srcIt = m_dirents.find(srcNormalized);
    if (srcIt == m_dirents.end() || srcIt->second.isDirectory) {
        throw IOException("copyFile", std::string(src), "Source is not a file");
    }
    
    // Check if destination already exists
    if (ensurePathIndexed(destNormalized)) {
        throw IOException("copyFile", std::string(dest), "Destination already exists");
    }
    
    // Read source file data
    std::vector<uint8_t> data = readFile(src);
    
    // Write to destination
    writeFileUnchecked(destNormalized, data);
}

void ExfatVolume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    const std::string srcNormalized = normalizeArg(src);
    const std::string destNormalized = normalizeArg(dest);
    
    // Check source file
    if (!ensurePathIndexed(srcNormalized)) {
        throw IOException("moveFile", std::string(src), "Source file does not exist");
    }
    
    auto srcIt = m_dirents.find(srcNormalized);
    if (srcIt == m_dirents.end() || srcIt->second.isDirectory) {
        throw IOException("moveFile", std::string(src), "Source is not a file");
    }
    
    // Check if destination already exists
    if (ensurePathIndexed(destNormalized)) {
        throw IOException("moveFile", std::string(dest), "Destination already exists");
    }
    
    // Read source file data
    std::vector<uint8_t> data = readFile(src);
    
    // Write to destination
    writeFileUnchecked(destNormalized, data);
    
    // Remove source
    removeFileThrowsUnchecked(src);
}

void ExfatVolume::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) {
    const std::string oldNormalized = normalizeArg(oldPath);
    const std::string newNormalized = normalizeArg(newPath);
    
    // Check source
    if (!ensurePathIndexed(oldNormalized)) {
        throw IOException("renameFile", std::string(oldPath), "Source does not exist");
    }
    
    auto oldIt = m_dirents.find(oldNormalized);
    if (oldIt == m_dirents.end()) {
        throw IOException("renameFile", std::string(oldPath), "Source does not exist");
    }
    
    // Check if new path already exists
    if (ensurePathIndexed(newNormalized)) {
        throw IOException("renameFile", std::string(newPath), "Destination already exists");
    }
    
    // Check if parent directory exists for new path
    const std::string newParent = getParentPath(newNormalized);
    if (!ensurePathIndexed(newParent)) {
        throw IOException("renameFile", std::string(newPath), "Parent directory does not exist");
    }
    
    // For exFAT, rename is implemented as copy + delete
    // A more efficient implementation would update the filename entry in place
    if (oldIt->second.isDirectory) {
        // For directories, we'd need to update parent's children
        // For now, throw not implemented
        throw IOException("renameFile", std::string(oldPath), "Directory rename not yet implemented");
    }
    
    // Read file data
    std::vector<uint8_t> data = readFile(oldPath);
    
    // Write to new location
    writeFileUnchecked(newNormalized, data);
    
    // Remove old entry
    removeFileThrowsUnchecked(oldPath);
}

uint32_t ExfatVolume::findFreeCluster() const {
    // Read allocation bitmap
    if (m_bitmapCluster < 2) {
        return 0;
    }
    
    uint64_t bitmapOffset = clusterToOffset(m_bitmapCluster);
    std::vector<uint8_t> bitmap(m_bitmapLength);
    
    if (!m_device->read(bitmapOffset, bitmap.data(), bitmap.size())) {
        return 0;
    }
    
    // Find first free cluster (bit = 0)
    for (uint32_t i = 0; i < m_clusterCount && i < m_bitmapLength * 8; ++i) {
        uint32_t byteIndex = i / 8;
        uint32_t bitIndex = i % 8;
        
        if ((bitmap[byteIndex] & (1 << bitIndex)) == 0) {
            return i + 2;  // Cluster numbers start at 2
        }
    }
    
    return 0;  // No free cluster found
}

uint32_t ExfatVolume::allocateCluster(uint32_t prevCluster) {
    uint32_t cluster = findFreeCluster();
    if (cluster == 0) {
        return 0;
    }
    
    // Mark cluster as allocated in bitmap
    uint32_t clusterIndex = cluster - 2;
    uint32_t byteIndex = clusterIndex / 8;
    uint32_t bitIndex = clusterIndex % 8;
    
    uint64_t bitmapOffset = clusterToOffset(m_bitmapCluster) + byteIndex;
    uint8_t bitmapByte;
    m_device->read(bitmapOffset, &bitmapByte, 1);
    bitmapByte |= (1 << bitIndex);
    m_device->write(bitmapOffset, &bitmapByte, 1);
    
    // Update FAT entry
    setFatEntry(cluster, 0xFFFFFFFF);  // Mark as end of chain
    
    // Link to previous cluster if exists
    if (prevCluster != 0) {
        setFatEntry(prevCluster, cluster);
    }
    
    return cluster;
}

void ExfatVolume::freeClusterChain(uint32_t firstCluster) {
    uint32_t cluster = firstCluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = getFatEntry(cluster);
        
        // Mark cluster as free in bitmap
        uint32_t clusterIndex = cluster - 2;
        uint32_t byteIndex = clusterIndex / 8;
        uint32_t bitIndex = clusterIndex % 8;
        
        uint64_t bitmapOffset = clusterToOffset(m_bitmapCluster) + byteIndex;
        uint8_t bitmapByte;
        m_device->read(bitmapOffset, &bitmapByte, 1);
        bitmapByte &= ~(1 << bitIndex);
        m_device->write(bitmapOffset, &bitmapByte, 1);
        
        // Clear FAT entry
        setFatEntry(cluster, 0);
        
        if (next >= 0x0FFFFFF8 || next == cluster) {
            break;
        }
        cluster = next;
    }
}

void ExfatVolume::createFileEntry(std::string_view path, uint32_t firstCluster, uint64_t size) {
    const std::string normalized = normalizeArg(path);
    const std::string parent = getParentPath(normalized);
    std::string fileName = getFileName(normalized);
    
    // Find parent directory
    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        throw IOException("createFileEntry", std::string(path), "Parent directory not found");
    }
    
    uint32_t parentCluster = pit->second.firstCluster;
    if (parent == "/" && parentCluster == 0) {
        parentCluster = m_rootCluster;
    }
    
    // Prepare directory entries (File + Stream + Filename)
    std::vector<std::vector<uint8_t>> entries;
    
    // File entry (32 bytes)
    std::vector<uint8_t> fileEntry(32, 0);
    fileEntry[0] = 0x85;  // File entry type
    fileEntry[1] = 0x00;  // No special attributes
    uint32_t timestamp = getExfatTimestamp();
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 8) = timestamp;   // Create time
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 12) = timestamp;  // Modify time
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 16) = timestamp;  // Access time
    entries.push_back(fileEntry);
    
    // Stream entry (32 bytes)
    std::vector<uint8_t> streamEntry(32, 0);
    streamEntry[0] = 0xC0;  // Stream entry type
    *reinterpret_cast<uint32_t*>(streamEntry.data() + 20) = firstCluster;
    *reinterpret_cast<uint64_t*>(streamEntry.data() + 24) = size;
    entries.push_back(streamEntry);
    
    // Filename entry (variable length, padded to 32 bytes)
    std::vector<uint8_t> nameEntry(32, 0);
    nameEntry[0] = 0xC1;  // Filename entry type
    nameEntry[1] = fileName.size();  // Name length
    
    // Convert filename to UTF-16LE
    for (size_t i = 0; i < fileName.size() && i < 30; ++i) {
        nameEntry[2 + i * 2] = fileName[i];
        nameEntry[3 + i * 2] = 0;
    }
    entries.push_back(nameEntry);
    
    // Write entries to parent directory
    // For simplicity, we append to the end of the directory
    // A full implementation would find free slots
    writeDirectoryEntries(parentCluster, entries);
    
    // Update in-memory cache
    Dirent dirent;
    dirent.isDirectory = false;
    dirent.firstCluster = firstCluster;
    dirent.size = size;
    dirent.mtime = timestamp >> 16;
    dirent.ctime = timestamp >> 16;
    dirent.atime = timestamp;
    m_dirents[normalized] = dirent;
    pit->second.children[fileName] = &m_dirents[normalized];
}

void ExfatVolume::createDirectoryEntry(std::string_view path, uint32_t firstCluster) {
    const std::string normalized = normalizeArg(path);
    const std::string parent = getParentPath(normalized);
    std::string dirName = getFileName(normalized);
    
    // Find parent directory
    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        throw IOException("createDirectoryEntry", std::string(path), "Parent directory not found");
    }
    
    uint32_t parentCluster = pit->second.firstCluster;
    if (parent == "/" && parentCluster == 0) {
        parentCluster = m_rootCluster;
    }
    
    // Prepare directory entries
    std::vector<std::vector<uint8_t>> entries;
    
    // File entry
    std::vector<uint8_t> fileEntry(32, 0);
    fileEntry[0] = 0x85;
    fileEntry[1] = 0x10;  // Directory attribute
    uint32_t timestamp = getExfatTimestamp();
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 8) = timestamp;
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 12) = timestamp;
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 16) = timestamp;
    entries.push_back(fileEntry);
    
    // Stream entry
    std::vector<uint8_t> streamEntry(32, 0);
    streamEntry[0] = 0xC0;
    *reinterpret_cast<uint32_t*>(streamEntry.data() + 20) = firstCluster;
    *reinterpret_cast<uint64_t*>(streamEntry.data() + 24) = 0;  // Directories have size 0
    entries.push_back(streamEntry);
    
    // Filename entry
    std::vector<uint8_t> nameEntry(32, 0);
    nameEntry[0] = 0xC1;
    nameEntry[1] = dirName.size();
    for (size_t i = 0; i < dirName.size() && i < 30; ++i) {
        nameEntry[2 + i * 2] = dirName[i];
        nameEntry[3 + i * 2] = 0;
    }
    entries.push_back(nameEntry);
    
    // Write entries
    writeDirectoryEntries(parentCluster, entries);
    
    // Update cache
    Dirent dirent;
    dirent.isDirectory = true;
    dirent.firstCluster = firstCluster;
    dirent.size = 0;
    dirent.mtime = timestamp >> 16;
    dirent.ctime = timestamp >> 16;
    dirent.atime = timestamp;
    m_dirents[normalized] = dirent;
    pit->second.children[dirName] = &m_dirents[normalized];
}

void ExfatVolume::deleteFileEntry(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    if (it != m_dirents.end() && !it->second.isDirectory) {
        // Mark entry as deleted in parent directory
        // For now, just remove from cache
        // A full implementation would mark the entry as 0xE5 on disk
        
        const std::string parent = getParentPath(normalized);
        auto pit = m_dirents.find(parent);
        if (pit != m_dirents.end() && pit->second.isDirectory) {
            std::string fileName = getFileName(normalized);
            pit->second.children.erase(fileName);
        }
        
        m_dirents.erase(it);
    }
}

void ExfatVolume::deleteDirectoryEntry(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    if (it != m_dirents.end() && it->second.isDirectory) {
        // Mark entry as deleted
        const std::string parent = getParentPath(normalized);
        auto pit = m_dirents.find(parent);
        if (pit != m_dirents.end() && pit->second.isDirectory) {
            std::string dirName = getFileName(normalized);
            pit->second.children.erase(dirName);
        }
        
        m_dirents.erase(it);
    }
}

void ExfatVolume::writeDirectoryEntries(uint32_t dirCluster, const std::vector<std::vector<uint8_t>>& entries) {
    // Find free space in directory
    // For simplicity, append to end
    // A full implementation would find and reuse deleted entries
    
    std::vector<uint32_t> chain = readClusterChain(dirCluster);
    if (chain.empty()) {
        throw IOException("writeDirectoryEntries", "", "Directory has no clusters");
    }
    
    // Calculate total size needed
    size_t totalSize = 0;
    for (const auto& entry : entries) {
        totalSize += entry.size();
    }
    
    // Find position to write (end of directory)
    // For now, just write to first cluster after existing entries
    // This is a simplification - a full implementation would properly extend the directory
    
    uint64_t offset = clusterToOffset(chain[0]);
    
    // Write entries
    for (const auto& entry : entries) {
        if (!writeAt(offset, entry.data(), entry.size())) {
            throw IOException("writeDirectoryEntries", "", "Failed to write directory entry");
        }
        offset += entry.size();
    }
}
