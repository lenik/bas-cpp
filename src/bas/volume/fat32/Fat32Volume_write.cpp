#include "Fat32Volume.hpp"

#include "fat32_utils.hpp"

#include "../../io/IOException.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

void Fat32Volume::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("writeFile", std::string(path), "Cannot write to root directory");
    }

    // Check if file already exists
    auto it = m_dirents.find(normalized);
    if (it != m_dirents.end() && !it->second.isDirectory) {
        // Update existing file - free old clusters
        freeClusterChain(it->second.firstCluster);
    }

    if (data.empty()) {
        // Create empty file
        createFileEntry(normalized, 0, 0);
        return;
    }

    // Allocate clusters for new data
    const uint32_t numClusters = (data.size() + m_clusterSize - 1) / m_clusterSize;
    uint32_t firstCluster = 0;
    uint32_t prevCluster = 0;

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

    // Create or update directory entry
    updateFileEntry(normalized, firstCluster, static_cast<uint32_t>(data.size()));
}

void Fat32Volume::createDirectoryThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("createDirectory", std::string(path), "Cannot create root directory");
    }

    // Check if path already exists
    auto it = m_dirents.find(normalized);
    if (it != m_dirents.end()) {
        throw IOException("createDirectory", std::string(path), "Path already exists");
    }

    // Check if parent directory exists
    const std::string parent = getParentPath(normalized);
    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        throw IOException("createDirectory", std::string(path), "Parent directory does not exist");
    }

    // Allocate a cluster for the new directory (empty directory with . and ..)
    uint32_t firstCluster = allocateCluster(0);
    if (firstCluster == 0) {
        throw IOException("createDirectory", std::string(path), "No space left on device");
    }

    // Create empty directory cluster (just mark end of chain)
    setFatEntry(firstCluster, 0x0FFFFFF8);

    // Write empty cluster (in real implementation, would write . and .. entries)
    std::vector<uint8_t> emptyCluster(m_clusterSize, 0);
    if (!writeAt(clusterToOffset(firstCluster), emptyCluster.data(), emptyCluster.size())) {
        freeClusterChain(firstCluster);
        throw IOException("createDirectory", std::string(path),
                          "Failed to write directory cluster");
    }

    // Create directory entry
    createDirectoryEntry(normalized, firstCluster);
}

void Fat32Volume::removeDirectoryThrowsUnchecked(std::string_view path) {
    std::string resolved = resolvePath(path);
    if (resolved.empty() || resolved == "/") {
        throw IOException("removeDirectory", std::string(path), "Cannot remove root directory or directory does not exist");
    }

    auto it = m_dirents.find(resolved);
    if (it == m_dirents.end() || !it->second.isDirectory) {
        throw IOException("removeDirectory", std::string(path), "Directory does not exist");
    }

    // Check if directory is empty (no children)
    const std::string prefix = resolved + "/";
    for (const auto& [fullPath, dirent] : m_dirents) {
        if (fullPath != resolved && fullPath.rfind(prefix, 0) == 0) {
            throw IOException("removeDirectory", std::string(path), "Directory is not empty");
        }
    }

    // Free cluster chain
    freeClusterChain(it->second.firstCluster);

    // Remove from index
    deleteDirectoryEntry(resolved);
}

void Fat32Volume::removeFileThrowsUnchecked(std::string_view path) {
    std::string resolved = resolvePath(path);
    if (resolved.empty()) {
        throw IOException("removeFile", std::string(path), "File does not exist");
    }

    auto it = m_dirents.find(resolved);
    if (it == m_dirents.end() || it->second.isDirectory) {
        throw IOException("removeFile", std::string(path), "File does not exist or is a directory");
    }

    // Free cluster chain
    freeClusterChain(it->second.firstCluster);

    // Remove from index
    deleteFileEntry(resolved);
}

void Fat32Volume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    std::string resolvedSrc = resolvePath(src);
    if (resolvedSrc.empty()) {
        throw IOException("copyFile", std::string(src), "Source file does not exist");
    }
    
    auto srcIt = m_dirents.find(resolvedSrc);
    if (srcIt == m_dirents.end() || srcIt->second.isDirectory) {
        throw IOException("copyFile", std::string(src),
                          "Source file does not exist or is a directory");
    }

    // Check if destination already exists
    std::string resolvedDest = resolvePath(dest);
    if (!resolvedDest.empty()) {
        throw IOException("copyFile", std::string(dest), "Destination already exists");
    }

    // Read source file data
    std::vector<uint8_t> data = readFile(src);

    // Write to destination
    writeFileUnchecked(dest, data);
}

void Fat32Volume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    std::string resolvedSrc = resolvePath(src);
    if (resolvedSrc.empty()) {
        throw IOException("moveFile", std::string(src), "Source file does not exist");
    }
    
    auto srcIt = m_dirents.find(resolvedSrc);
    if (srcIt == m_dirents.end() || srcIt->second.isDirectory) {
        throw IOException("moveFile", std::string(src),
                          "Source file does not exist or is a directory");
    }

    // Check if destination already exists
    std::string resolvedDest = resolvePath(dest);
    if (!resolvedDest.empty()) {
        throw IOException("moveFile", std::string(dest), "Destination already exists");
    }

    // Read source file data
    std::vector<uint8_t> data = readFile(src);

    // Write to destination
    writeFileUnchecked(dest, data);

    // Remove source
    removeFileThrowsUnchecked(src);
    
    // Invalidate index to force re-read from disk with updated state
    invalidateIndex();
}

void Fat32Volume::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) {
    const std::string oldNormalized = normalizeArg(oldPath);
    const std::string newNormalized = normalizeArg(newPath);

    // Find old entry using flexible lookup
    std::string actualOldPath = oldNormalized;
    auto oldIt = m_dirents.find(actualOldPath);
    if (oldIt == m_dirents.end()) {
        // Try short name version
        actualOldPath = toShortNamePath(oldNormalized);
        oldIt = m_dirents.find(actualOldPath);
    }
    if (oldIt == m_dirents.end()) {
        throw IOException("renameFile", std::string(oldPath), "Source does not exist");
    }

    // Check if new path already exists (try both long and short)
    std::string actualNewPath = newNormalized;
    auto newIt = m_dirents.find(actualNewPath);
    if (newIt == m_dirents.end()) {
        actualNewPath = toShortNamePath(newNormalized);
        newIt = m_dirents.find(actualNewPath);
    }
    if (newIt != m_dirents.end()) {
        throw IOException("renameFile", std::string(newPath), "Destination already exists");
    }

    // Get parent directory for new path
    const std::string newParent = getParentPath(newNormalized);
    auto parentIt = m_dirents.find(newParent);
    if (parentIt == m_dirents.end() || !parentIt->second.isDirectory) {
        throw IOException("renameFile", std::string(newPath), "Parent directory does not exist");
    }

    // Create new entry with same data (use short name for consistency)
    Dirent dirent = oldIt->second;
    
    // Convert new path to short name for disk consistency
    std::string newFileName = getFileName(newNormalized);
    newFileName = createShortName(newFileName);
    const std::string newShortPath = (newParent == "/") ? ("/" + newFileName) : (newParent + "/" + newFileName);
    
    m_dirents[newShortPath] = dirent;
    
    // Write new entry to disk
    writeDirectoryEntryToDisk(newShortPath, dirent);

    // Get old entry info before erasing
    const std::string oldParent = getParentPath(actualOldPath);
    std::string oldFileName = getFileName(actualOldPath);
    // Convert to short name to match what's on disk
    oldFileName = createShortName(oldFileName);
    
    // Mark old entry as deleted on disk FIRST (before erasing from memory)
    auto oldParentIt = m_dirents.find(oldParent);
    if (oldParentIt != m_dirents.end() && oldParentIt->second.isDirectory) {
        uint32_t oldParentCluster = oldParentIt->second.firstCluster;
        if (oldParent == "/" && oldParentCluster == 0) {
            oldParentCluster = m_rootCluster;
        }
        markDirectoryEntryAsDeleted(oldParentCluster, oldFileName);
    }
    
    // Remove old entry from memory
    m_dirents.erase(actualOldPath);
    
    // Invalidate index to force re-read from disk with updated state
    invalidateIndex();
}

bool Fat32Volume::writeAt(uint64_t offset, const uint8_t* src, size_t len) {
    if (!src || len == 0 || offset + len > m_imageSize) {
        return false;
    }
    std::ofstream out(m_imagePath, std::ios::binary | std::ios::in | std::ios::out);
    if (!out.is_open()) {
        return false;
    }
    out.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!out.good()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(src), static_cast<std::streamsize>(len));
    out.flush();
    if (!out.good()) {
        return false;
    }
    out.close();
    return true;
}

uint32_t Fat32Volume::findFreeCluster() const {
    for (uint32_t cluster = 2; cluster < 0x0FFFFFF8; ++cluster) {
        uint32_t entry = getFatEntry(cluster);
        if (entry == 0) {
            return cluster;
        }
    }
    return 0; // No free cluster found
}

uint32_t Fat32Volume::allocateCluster(uint32_t prevCluster) {
    uint32_t cluster = findFreeCluster();
    if (cluster == 0) {
        return 0;
    }

    // Mark cluster as end of chain
    setFatEntry(cluster, 0x0FFFFFF8);

    // Link to previous cluster if exists
    if (prevCluster != 0) {
        setFatEntry(prevCluster, cluster);
    }

    return cluster;
}

void Fat32Volume::freeClusterChain(uint32_t firstCluster) {
    uint32_t cluster = firstCluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = getFatEntry(cluster);
        setFatEntry(cluster, 0); // Mark as free
        if (next >= 0x0FFFFFF8 || next == cluster) {
            break;
        }
        cluster = next;
    }
}

void Fat32Volume::setFatEntry(uint32_t cluster, uint32_t value) {
    const uint64_t off = m_fatOffset + static_cast<uint64_t>(cluster) * 4;
    if (off + 4 > m_imageSize) {
        throw IOException("Fat32Volume", m_imagePath, "FAT entry out of range");
    }
    uint8_t b[4];
    b[0] = value & 0xFF;
    b[1] = (value >> 8) & 0xFF;
    b[2] = (value >> 16) & 0xFF;
    b[3] = (value >> 24) & 0xFF;
    writeAt(off, b, sizeof(b));
}

void Fat32Volume::createFileEntry(std::string_view path, uint32_t firstCluster, uint32_t size) {
    const std::string normalized = normalizeArg(path);
    const std::string parent = getParentPath(normalized);
    std::string fileName = getFileName(normalized);

    if (fileName.empty()) {
        throw IOException("createFileEntry", std::string(path), "Invalid file name");
    }

    // Find parent directory
    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        throw IOException("createFileEntry", std::string(path), "Parent directory not found");
    }

    // Convert to short name for consistency with disk storage
    fileName = createShortName(fileName);
    const std::string shortPath = (parent == "/") ? ("/" + fileName) : (parent + "/" + fileName);

    // Update in-memory index with short name
    time_t now = getCurrentTime();
    m_dirents[shortPath] = Dirent{false, firstCluster, size, now, now, now};

    // Write directory entry to disk
    writeDirectoryEntryToDisk(shortPath, m_dirents[shortPath]);
}

void Fat32Volume::updateFileEntry(std::string_view path, uint32_t firstCluster, uint32_t size) {
    createFileEntry(path, firstCluster, size);
}

void Fat32Volume::deleteFileEntry(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end()) {
        return; // Entry not found, nothing to do
    }
    if (it->second.isDirectory) {
        return; // Can't delete directory with deleteFileEntry
    }
    
    // Mark directory entry as deleted on disk
    const std::string parent = getParentPath(normalized);
    const std::string fileName = getFileName(normalized);
    auto pit = m_dirents.find(parent);
    if (pit != m_dirents.end() && pit->second.isDirectory) {
        uint32_t parentCluster = pit->second.firstCluster;
        if (parent == "/" && parentCluster == 0) {
            parentCluster = m_rootCluster;
        }
        markDirectoryEntryAsDeleted(parentCluster, fileName);
    }
    
    m_dirents.erase(it);
}

void Fat32Volume::createDirectoryEntry(std::string_view path, uint32_t firstCluster) {
    const std::string normalized = normalizeArg(path);
    const std::string parent = getParentPath(normalized);
    std::string dirName = getFileName(normalized);
    
    // Convert to short name for consistency with disk storage
    dirName = createShortName(dirName);
    const std::string shortPath = (parent == "/") ? ("/" + dirName) : (parent + "/" + dirName);
    
    time_t now = getCurrentTime();
    m_dirents[shortPath] = Dirent{true, firstCluster, 0, now, now, now};

    // Write directory entry to disk
    writeDirectoryEntryToDisk(shortPath, m_dirents[shortPath]);
}

void Fat32Volume::deleteDirectoryEntry(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    if (it != m_dirents.end() && it->second.isDirectory) {
        // Mark directory entry as deleted on disk
        const std::string parent = getParentPath(normalized);
        const std::string fileName = getFileName(normalized);
        auto pit = m_dirents.find(parent);
        if (pit != m_dirents.end() && pit->second.isDirectory) {
            uint32_t parentCluster = pit->second.firstCluster;
            if (parent == "/" && parentCluster == 0) {
                parentCluster = m_rootCluster;
            }
            markDirectoryEntryAsDeleted(parentCluster, fileName);
        }
        
        m_dirents.erase(normalized);
    }
}

uint32_t Fat32Volume::findFreeDirEntrySlot(uint32_t dirCluster) {
    // Read directory clusters and find free slot (marked with 0x00 or 0xE5)
    std::vector<uint32_t> chain = readClusterChain(dirCluster);

    uint32_t clusterIndex = 0;
    for (uint32_t cluster : chain) {
        uint64_t offset = clusterToOffset(cluster);
        std::vector<uint8_t> block(m_clusterSize);

        if (!readAt(offset, block.data(), block.size())) {
            clusterIndex++;
            continue;
        }

        // Scan for free entry
        for (uint32_t pos = 0; pos + 32 <= m_clusterSize; pos += 32) {
            uint8_t firstByte = block[pos];
            if (firstByte == 0x00 || firstByte == 0xE5) {
                // Found free slot - return absolute byte offset in cluster chain
                return (clusterIndex * m_clusterSize) + pos;
            }
        }
        clusterIndex++;
    }

    return 0xFFFFFFFF; // No free slot found
}

void Fat32Volume::expandDirectoryIfNeeded(uint32_t dirCluster) {
    // Check if directory has space for at least one more entry (32 bytes)
    std::vector<uint32_t> chain = readClusterChain(dirCluster);

    if (chain.empty()) {
        // Allocate first cluster for directory
        uint32_t newCluster = allocateCluster(0);
        if (newCluster == 0) {
            throw IOException("expandDirectory", m_imagePath, "No space for directory expansion");
        }

        // Clear the cluster
        std::vector<uint8_t> emptyCluster(m_clusterSize, 0);
        writeAt(clusterToOffset(newCluster), emptyCluster.data(), emptyCluster.size());
    }
    // For simplicity, we don't handle multi-cluster directory expansion here
    // A full implementation would add new clusters to the chain
}

void Fat32Volume::writeDirectoryEntryToDisk(std::string_view path, const Dirent& dirent) {
    const std::string normalized = normalizeArg(path);
    const std::string parent = getParentPath(normalized);
    std::string fileName = getFileName(normalized);

    if (fileName.empty()) {
        throw IOException("writeDirectoryEntryToDisk", std::string(path), "Invalid file name");
    }

    // Find parent directory
    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        throw IOException("writeDirectoryEntryToDisk", std::string(path),
                          "Parent directory not found");
    }

    uint32_t parentCluster = pit->second.firstCluster;

    // Handle root directory specially (cluster 0 or 2)
    if (parent == "/" && parentCluster == 0) {
        parentCluster = m_rootCluster;
    }

    // Ensure directory has space
    expandDirectoryIfNeeded(parentCluster);

    // Find free slot - returns byte offset within cluster chain
    uint32_t slotOffset = findFreeDirEntrySlot(parentCluster);
    if (slotOffset == 0xFFFFFFFF) {
        throw IOException("writeDirectoryEntryToDisk", std::string(path), "No space in directory");
    }

    // Write to disk - calculate which cluster and offset
    std::vector<uint32_t> chain = readClusterChain(parentCluster);
    uint32_t clusterIndex = slotOffset / m_clusterSize;
    uint32_t offsetInCluster = slotOffset % m_clusterSize;

    if (clusterIndex >= chain.size()) {
        throw IOException("writeDirectoryEntryToDisk", std::string(path), "Invalid cluster index");
    }

    // Use short name for disk storage (8.3 format)
    uint64_t clusterOffset = clusterToOffset(chain[clusterIndex]) + offsetInCluster;
    std::vector<uint8_t> entry(32);
    std::string shortName = createShortName(fileName);
    writeDirEntry(entry.data(), shortName, dirent.firstCluster, dirent.size, dirent.isDirectory,
                  dirent.mtime, dirent.atime, dirent.creationTime);
    if (!writeAt(clusterOffset, entry.data(), entry.size())) {
        throw IOException("writeDirectoryEntryToDisk", std::string(path),
                          "Failed to write entry to disk");
    }
}

void Fat32Volume::updateDirectoryEntryOnDisk(std::string_view path, const Dirent& dirent) {
    // For now, just write as new entry
    // A full implementation would find and update the existing entry
    writeDirectoryEntryToDisk(path, dirent);
}

void Fat32Volume::markDirectoryEntryAsDeleted(uint32_t dirCluster, std::string_view name) {
    // Mark first byte of directory entry as 0xE5 (deleted)
    std::vector<uint32_t> chain = readClusterChain(dirCluster);
    
    // Convert to short name for matching
    std::string shortName = createShortName(std::string(name));
    
    // Build 11-byte FAT name (uppercase, as stored on disk)
    uint8_t fatName[11];
    memset(fatName, ' ', 11);
    
    std::string base = shortName;
    std::string ext = "";
    size_t dotPos = shortName.find('.');
    if (dotPos != std::string::npos && dotPos < shortName.size() - 1) {
        ext = shortName.substr(dotPos + 1);
        base = shortName.substr(0, dotPos);
    }
    
    // Copy base (up to 8 chars, uppercase)
    for (size_t i = 0; i < 8 && i < base.size(); ++i) {
        fatName[i] = static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(base[i])));
    }
    
    // Copy extension (up to 3 chars, uppercase)
    for (size_t i = 0; i < 3 && i < ext.size(); ++i) {
        fatName[8 + i] = static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(ext[i])));
    }
    
    for (uint32_t cluster : chain) {
        uint64_t offset = clusterToOffset(cluster);
        std::vector<uint8_t> block(m_clusterSize);
        
        if (!readAt(offset, block.data(), block.size())) {
            continue;
        }
        
        // Scan for matching entry
        for (uint32_t pos = 0; pos + 32 <= m_clusterSize; pos += 32) {
            if (block[pos] == 0x00) {
                // End of directory entries
                return;
            }
            
            if (block[pos] == 0xE5 || block[pos] == 0x05) {
                // Deleted entry, skip
                continue;
            }
            
            // Check if this is an LFN entry
            if (block[11] == 0x0F) {
                continue;
            }
            
            // Compare name (11 bytes, uppercase on disk)
            bool matches = true;
            for (int i = 0; i < 11; ++i) {
                if (block[pos + i] != fatName[i]) {
                    matches = false;
                    break;
                }
            }
            
            if (matches) {
                // Mark as deleted
                block[pos] = 0xE5;
                if (!writeAt(offset + pos, block.data() + pos, 32)) {
                    throw IOException("markDirectoryEntryAsDeleted", m_imagePath, "Failed to write deletion marker");
                }
                return;
            }
        }
    }
}

uint8_t Fat32Volume::calculateLFNChecksumForShortName(const std::string& longName) {
    std::string shortName = createShortName(longName);
    return calculateLFNChecksum(shortName);
}

std::string Fat32Volume::createShortName(const std::string& longName) const {
    // Extract base name and extension
    std::string base = longName;
    std::string ext = "";
    
    size_t dotPos = longName.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < longName.size() - 1) {
        ext = longName.substr(dotPos + 1);
        base = longName.substr(0, dotPos);
    }
    
    // Truncate base to 6 chars + ~1 if needed
    if (base.size() > 8) {
        base = base.substr(0, 6) + "~1";
    }
    
    // Truncate extension to 3 chars
    if (ext.size() > 3) {
        ext = ext.substr(0, 3);
    }
    
    // Convert to lowercase to match decodeShortName output
    for (char& c : base)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    
    // Build full 8.3 name (no padding for in-memory use)
    std::string result = base;
    if (!ext.empty()) {
        result += ".";
        result += ext;
    }
    
    return result;
}

void Fat32Volume::writeLFNEntries(uint32_t dirCluster, uint32_t slotOffset,
                                  const std::string& longName, uint8_t checksum) {
    auto chunks = splitLFNChunks(longName);

    std::vector<uint32_t> chain = readClusterChain(dirCluster);

    for (size_t i = 0; i < chunks.size(); ++i) {
        uint8_t order = static_cast<uint8_t>(i + 1);
        if (i == 0) {
            order |= 0x40; // Mark last LFN entry
        }

        // Calculate position
        uint32_t absoluteOffset = slotOffset + ((chunks.size() - i - 1) * 32);
        uint32_t clusterIndex = absoluteOffset / m_clusterSize;
        uint32_t offsetInCluster = absoluteOffset % m_clusterSize;

        if (clusterIndex >= chain.size()) {
            throw IOException("writeLFNEntries", m_imagePath, "Invalid cluster index for LFN");
        }

        // Create LFN entry
        std::vector<uint8_t> entry(32, 0xFF);
        entry[0] = order;
        entry[11] = 0x0F; // LFN attribute
        entry[12] = checksum;
        entry[26] = 0;

        // Write name as UTF-16LE
        const std::string& chunk = chunks[i];
        size_t charIdx = 0;
        auto writeChar = [&](int offset) {
            if (charIdx < chunk.size()) {
                uint16_t ch = static_cast<unsigned char>(chunk[charIdx++]);
                entry[offset] = ch & 0xFF;
                entry[offset + 1] = (ch >> 8) & 0xFF;
            } else if (charIdx == chunk.size()) {
                entry[offset] = 0x00;
                entry[offset + 1] = 0x00;
                charIdx++;
            } else {
                entry[offset] = 0xFF;
                entry[offset + 1] = 0xFF;
            }
        };

        for (int j = 0; j < 5; ++j)
            writeChar(1 + j * 2);
        for (int j = 0; j < 6; ++j)
            writeChar(14 + j * 2);
        for (int j = 0; j < 2; ++j)
            writeChar(28 + j * 2);

        // Write to disk
        uint64_t clusterOffset = clusterToOffset(chain[clusterIndex]) + offsetInCluster;
        writeAt(clusterOffset, entry.data(), entry.size());
    }
}
