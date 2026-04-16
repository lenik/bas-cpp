#include "Fat32Volume.hpp"

#include "fat32_utils.hpp"

#include "../../io/IOException.hpp"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

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

    // Remove from parent's children list
    const std::string parent = getParentPath(resolved);
    auto parentIt = m_dirents.find(parent);
    if (parentIt != m_dirents.end() && parentIt->second.isDirectory) {
        std::string childName = getFileName(resolved);
        parentIt->second.children.erase(childName);
    }

    // Remove from index and mark on disk
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

    // Remove from parent's children list
    const std::string parent = getParentPath(resolved);
    auto parentIt = m_dirents.find(parent);
    if (parentIt != m_dirents.end() && parentIt->second.isDirectory) {
        std::string childName = getFileName(resolved);
        parentIt->second.children.erase(childName);
    }

    // Remove from index and mark on disk
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

    // Find old entry - it should exist with either long or short name
    std::string actualOldPath = oldNormalized;
    auto oldIt = m_dirents.find(actualOldPath);
    if (oldIt == m_dirents.end()) {
        actualOldPath = toShortNamePath(oldNormalized);
        oldIt = m_dirents.find(actualOldPath);
    }
    if (oldIt == m_dirents.end()) {
        throw IOException("renameFile", std::string(oldPath), "Source does not exist");
    }

    // Check destination doesn't exist
    std::string actualNewPath = newNormalized;
    auto newIt = m_dirents.find(actualNewPath);
    if (newIt == m_dirents.end()) {
        actualNewPath = toShortNamePath(newNormalized);
        newIt = m_dirents.find(actualNewPath);
    }
    if (newIt != m_dirents.end()) {
        throw IOException("renameFile", std::string(newPath), "Destination already exists");
    }

    // Validate parent directory
    const std::string newParent = getParentPath(newNormalized);
    auto parentIt = m_dirents.find(newParent);
    if (parentIt == m_dirents.end() || !parentIt->second.isDirectory) {
        throw IOException("renameFile", std::string(newPath), "Parent directory does not exist");
    }

    // Get old entry info BEFORE any modifications
    Dirent dirent = oldIt->second;
    const std::string oldParent = getParentPath(actualOldPath);
    // Extract the ACTUAL filename from the path we found it with
    std::string oldFileName = getFileName(actualOldPath);
    // This should already be the short name since that's how it's stored
    
    // Create new entry with short name
    std::string newFileName = getFileName(newNormalized);
    newFileName = createShortName(newFileName);
    const std::string newShortPath = (newParent == "/") ? ("/" + newFileName) : (newParent + "/" + newFileName);
    
    // Write new entry to disk
    writeDirectoryEntryToDisk(newShortPath, dirent);
    m_dirents[newShortPath] = dirent;
    
    // Remove old entry from parent's children list
    auto oldParentIt = m_dirents.find(oldParent);
    if (oldParentIt != m_dirents.end() && oldParentIt->second.isDirectory) {
        std::string oldChildName = getFileName(actualOldPath);
        oldParentIt->second.children.erase(oldChildName);
    }
    
    // Remove old from m_dirents
    m_dirents.erase(actualOldPath);
    
    // Mark old entry as deleted on disk
    if (oldParentIt != m_dirents.end() && oldParentIt->second.isDirectory) {
        uint32_t oldParentCluster = oldParentIt->second.firstCluster;
        if (oldParent == "/" && oldParentCluster == 0) {
            oldParentCluster = m_rootCluster;
        }
        markDirectoryEntryAsDeleted(oldParentCluster, oldFileName);
    }
    // In-memory state is now consistent - no need to re-scan from disk
}

bool Fat32Volume::writeAt(uint64_t offset, const uint8_t* src, size_t len) {
    if (!m_device || !src || len == 0 || offset + len > m_device->size()) {
        return false;
    }
    return m_device->write(offset, src, len);
}

uint32_t Fat32Volume::findFreeCluster() {
    return m_clusterManager->findFreeFromHint();
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

int64_t Fat32Volume::findContiguousFreeRun(uint32_t count) {
    return m_clusterManager->findContiguousFreeRun(count);
}

std::vector<uint32_t> Fat32Volume::allocateClusterChain(uint32_t count) {
    std::vector<uint32_t> out;
    if (count == 0) {
        return out;
    }
    // Fast path: reserve a contiguous run to minimize FAT updates and improve sequential I/O.
    const int64_t runStart = m_clusterManager->findContiguousFreeRun(count);
    if (runStart >= 2) {
        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t c = static_cast<uint32_t>(runStart) + i;
            const uint32_t next =
                (i + 1 < count) ? (static_cast<uint32_t>(runStart) + i + 1) : 0x0FFFFFF8;
            setFatEntry(c, next);
            out.push_back(c);
        }
        m_clusterManager->setAllocHint(static_cast<uint32_t>(runStart) + count);
        return out;
    }
    out.reserve(count);
    uint32_t prev = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t c = m_clusterManager->findFreeFromHint();
        if (c == 0) {
            if (!out.empty()) {
                freeClusterChain(out.front());
            }
            throw IOException("reallocClusters", m_device->uri(), "No space left on device");
        }
        setFatEntry(c, 0x0FFFFFF8);
        if (prev != 0) {
            setFatEntry(prev, c);
        }
        out.push_back(c);
        prev = c;
    }
    return out;
}

Fat32Volume::ReallocStrategy Fat32Volume::analyzeReallocStrategy(const std::vector<uint32_t>& clusters,
                                                                 uint32_t needClusters) {
    if (clusters.size() == needClusters) {
        return ReallocStrategy::KEEP_AS_IS;
    }
    if (clusters.size() > needClusters) {
        return ReallocStrategy::SHRINK_TAIL;
    }
    if (!clusters.empty()) {
        const uint32_t grow = needClusters - static_cast<uint32_t>(clusters.size());
        const uint32_t tail = clusters.back();
        bool contiguous = true;
        for (uint32_t i = 1; i <= grow; ++i) {
            if (getFatEntry(tail + i) != 0) {
                contiguous = false;
                break;
            }
        }
        if (contiguous) {
            return ReallocStrategy::EXTEND_TAIL_CONTIGUOUS;
        }
    }
    if (m_clusterManager->findContiguousFreeRun(needClusters) >= 2) {
        return ReallocStrategy::RELOCATE_CONTIGUOUS;
    }
    return ReallocStrategy::REALLOC_FRAGMENTED;
}

std::vector<uint32_t> Fat32Volume::reallocClusters(const std::vector<uint32_t>& clusters, uint64_t newSize) {
    const uint32_t needClusters =
        (newSize == 0) ? 0u : static_cast<uint32_t>((newSize + m_clusterSize - 1) / m_clusterSize);
    if (needClusters == 0) {
        if (!clusters.empty()) {
            freeClusterChain(clusters.front());
        }
        m_clusterManager->flushDirty();
        return {};
    }

    std::vector<uint32_t> out = clusters;
    const ReallocStrategy strategy = analyzeReallocStrategy(clusters, needClusters);
    if (strategy == ReallocStrategy::KEEP_AS_IS) {
        return out;
    }

    if (strategy == ReallocStrategy::SHRINK_TAIL) {
        const uint32_t keepLast = out[needClusters - 1];
        if (needClusters < out.size()) {
            freeClusterChain(out[needClusters]);
        }
        setFatEntry(keepLast, 0x0FFFFFF8);
        out.resize(needClusters);
        m_clusterManager->flushDirty();
        return out;
    }

    const uint32_t grow = needClusters - static_cast<uint32_t>(out.size());
    if (strategy == ReallocStrategy::EXTEND_TAIL_CONTIGUOUS && !out.empty()) {
        const uint32_t tail = out.back();
        uint32_t prev = tail;
        for (uint32_t i = 1; i <= grow; ++i) {
            const uint32_t c = tail + i;
            setFatEntry(prev, c);
            setFatEntry(c, 0x0FFFFFF8);
            out.push_back(c);
            prev = c;
        }
        m_clusterManager->flushDirty();
        return out;
    }

    if (strategy == ReallocStrategy::RELOCATE_CONTIGUOUS) {
        const int64_t runStart = m_clusterManager->findContiguousFreeRun(needClusters);
        std::vector<uint32_t> rebuilt;
        rebuilt.reserve(needClusters);
        for (uint32_t i = 0; i < needClusters; ++i) {
            rebuilt.push_back(static_cast<uint32_t>(runStart) + i);
        }
        for (uint32_t i = 0; i < needClusters; ++i) {
            const uint32_t c = rebuilt[i];
            if (getFatEntry(c) != 0) {
                rebuilt.clear();
                break;
            }
            const uint32_t next = (i + 1 < needClusters) ? rebuilt[i + 1] : 0x0FFFFFF8;
            setFatEntry(c, next);
        }
        if (!rebuilt.empty()) {
            if (!clusters.empty()) {
                freeClusterChain(clusters.front());
            }
            m_clusterManager->flushDirty();
            return rebuilt;
        }
    }

    // Fallback: fragmented allocation with general head.
    std::vector<uint32_t> rebuilt = allocateClusterChain(needClusters);
    if (!clusters.empty()) {
        freeClusterChain(clusters.front());
    }
    m_clusterManager->flushDirty();
    return rebuilt;
}

void Fat32Volume::setFatEntry(uint32_t cluster, uint32_t value) {
    m_clusterManager->set(cluster, value);
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
    return findFreeDirEntrySlots(dirCluster, 1);
}

uint32_t Fat32Volume::findFreeDirEntrySlots(uint32_t dirCluster, uint32_t count) {
    // Read directory clusters and find consecutive free slots
    std::vector<uint32_t> chain = readClusterChain(dirCluster);

    uint32_t clusterIndex = 0;
    uint32_t consecutiveFree = 0;
    uint32_t startOffset = 0xFFFFFFFF;
    
    for (uint32_t cluster : chain) {
        uint64_t offset = clusterToOffset(cluster);
        std::vector<uint8_t> block(m_clusterSize);

        if (!readAt(offset, block.data(), block.size())) {
            clusterIndex++;
            consecutiveFree = 0;
            startOffset = 0xFFFFFFFF;
            continue;
        }

        // Scan for consecutive free entries
        for (uint32_t pos = 0; pos + 32 <= m_clusterSize; pos += 32) {
            uint8_t firstByte = block[pos];
            if (firstByte == 0x00 || firstByte == 0xE5) {
                if (consecutiveFree == 0) {
                    startOffset = (clusterIndex * m_clusterSize) + pos;
                }
                consecutiveFree++;
                if (consecutiveFree >= count) {
                    return startOffset;
                }
            } else {
                consecutiveFree = 0;
                startOffset = 0xFFFFFFFF;
            }
        }
        clusterIndex++;
    }

    return 0xFFFFFFFF; // No consecutive free slots found
}

void Fat32Volume::expandDirectoryIfNeeded(uint32_t dirCluster) {
    // Check if directory has space for at least one more entry (32 bytes)
    std::vector<uint32_t> chain = readClusterChain(dirCluster);

    if (chain.empty()) {
        // Allocate first cluster for directory
        uint32_t newCluster = allocateCluster(0);
        if (newCluster == 0) {
            throw IOException("expandDirectory", m_device->uri(), "No space for directory expansion");
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

    if (m_clusterSize % 32 != 0) {
        throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Invalid FAT32 cluster size");
    }

    // We always need consecutive 32-byte slots for (LFN entries + short name entry).
    constexpr size_t ENTRY_SIZE = 32;
    const uint32_t slotsPerCluster = m_clusterSize / ENTRY_SIZE;

    const bool needsLFN = fileName.size() > 12 || fileName.find('.') != std::string::npos;
    const size_t lfnEntries = needsLFN ? ((fileName.size() + 12) / 13) : 0;
    const uint32_t requiredSlots = needsLFN ? static_cast<uint32_t>(lfnEntries + 1) : 1;

    // Ensure directory has at least one cluster.
    expandDirectoryIfNeeded(parentCluster);

    std::vector<uint32_t> chain = readClusterChain(parentCluster);
    if (chain.empty()) {
        throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Directory has no clusters");
    }

    // ---- Placement policy (same spirit as exFAT):
    // 1) Prefer contiguous empty slots starting from the first 0x00 entry (end-of-entries marker).
    // 2) If directory page is full, backfill contiguous deleted slots (0xE5 / 0x05) before that marker.
    // 3) If still full, extend the directory cluster chain and write into the newly added space.

    const size_t totalSlots = chain.size() * static_cast<size_t>(slotsPerCluster);
    size_t emptyStartSlotAbs = static_cast<size_t>(-1);
    size_t emptyRunLen = 0;
    bool emptyRunReachesEnd = false;

    size_t deletedCandidateStartSlotAbs = 0;
    bool deletedCandidateFound = false;
    size_t deletedRunLen = 0;
    size_t deletedRunStartSlotAbs = 0;

    bool emptyFound = false;
    bool emptyRunBroke = false;

    size_t slotAbs = 0;
    for (size_t ci = 0; ci < chain.size() && !emptyRunBroke; ++ci) {
        std::vector<uint8_t> block(m_clusterSize, 0);
        const uint64_t baseOff = clusterToOffset(chain[ci]);
        if (!readAt(baseOff, block.data(), block.size())) {
            throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Failed to read directory cluster");
        }

        for (uint32_t si = 0; si < slotsPerCluster; ++si) {
            const uint8_t firstByte = block[static_cast<size_t>(si) * ENTRY_SIZE];

            if (!emptyFound) {
                if (firstByte == 0x00) {
                    emptyFound = true;
                    emptyStartSlotAbs = slotAbs;
                    emptyRunLen = 1;
                    deletedRunLen = 0;
                    continue;
                }
                if (firstByte == 0xE5 || firstByte == 0x05) {
                    if (deletedRunLen == 0) deletedRunStartSlotAbs = slotAbs;
                    deletedRunLen++;
                    if (!deletedCandidateFound && deletedRunLen >= requiredSlots) {
                        deletedCandidateStartSlotAbs = deletedRunStartSlotAbs;
                        deletedCandidateFound = true;
                    }
                } else {
                    deletedRunLen = 0;
                }
            } else {
                if (firstByte == 0x00) {
                    emptyRunLen++;
                } else {
                    emptyRunBroke = true;
                    emptyRunReachesEnd = false;
                    break;
                }
            }

            ++slotAbs;
        }
    }

    if (emptyFound && !emptyRunBroke) {
        emptyRunReachesEnd = true;
    }

    size_t startSlotAbs = 0;
    bool placementInExisting = false;
    if (emptyFound && emptyRunLen >= requiredSlots) {
        startSlotAbs = emptyStartSlotAbs;
        placementInExisting = true;
    } else if (deletedCandidateFound) {
        startSlotAbs = deletedCandidateStartSlotAbs;
        placementInExisting = true;
    } else {
        // Need to extend cluster chain.
        if (emptyFound && emptyRunReachesEnd) {
            startSlotAbs = emptyStartSlotAbs;
        } else {
            startSlotAbs = totalSlots; // first slot in the newly appended area
        }
    }

    // If we chose an existing start slot, make sure it can fit (it should for empty/deleted placement).
    // For extension placement, we ensure there are enough slots after appending.
    const size_t availableSlots = (startSlotAbs <= totalSlots) ? (totalSlots - startSlotAbs) : 0;
    size_t missingSlots = 0;
    if (!placementInExisting) {
        missingSlots = requiredSlots - availableSlots;
    }

    if (!placementInExisting && missingSlots > 0) {
        const size_t extraClustersNeeded =
            (missingSlots + slotsPerCluster - 1) / slotsPerCluster;

        uint32_t prev = chain.back();
        const std::vector<uint8_t> zeroCluster(m_clusterSize, 0);
        for (size_t i = 0; i < extraClustersNeeded; ++i) {
            uint32_t newCluster = allocateCluster(prev);
            if (newCluster == 0) {
                throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "No space left on device");
            }
            if (!writeAt(clusterToOffset(newCluster), zeroCluster.data(), zeroCluster.size())) {
                throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Failed to init extended directory cluster");
            }
            chain.push_back(newCluster);
            prev = newCluster;
        }
        m_clusterManager->flushDirty();
    }

    // Reload chain to keep clusterIndex computations consistent with the extended chain.
    chain = readClusterChain(parentCluster);
    const uint64_t insertByteOffset = static_cast<uint64_t>(startSlotAbs) * ENTRY_SIZE;

    if (needsLFN) {
        const uint8_t checksum = calculateLFNChecksumForShortName(fileName);
        writeLFNEntries(parentCluster, static_cast<uint32_t>(insertByteOffset), fileName, checksum);

        const uint64_t shortNameSlotByteOffset = insertByteOffset + static_cast<uint64_t>(lfnEntries) * ENTRY_SIZE;
        const uint32_t shortClusterIndex = static_cast<uint32_t>(shortNameSlotByteOffset / m_clusterSize);
        const uint32_t shortOffsetInCluster = static_cast<uint32_t>(shortNameSlotByteOffset % m_clusterSize);
        if (shortClusterIndex >= chain.size()) {
            throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Invalid cluster index for short name");
        }
        const uint64_t clusterOffset = clusterToOffset(chain[shortClusterIndex]) + shortOffsetInCluster;

        std::vector<uint8_t> entry(ENTRY_SIZE, 0);
        const std::string shortName = createShortName(fileName);
        writeDirEntry(entry.data(), shortName, dirent.firstCluster, dirent.size, dirent.isDirectory,
                      dirent.mtime, dirent.atime, dirent.creationTime);
        if (!writeAt(clusterOffset, entry.data(), entry.size())) {
            throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Failed to write short name entry to disk");
        }
    } else {
        const uint32_t clusterIndex = static_cast<uint32_t>(insertByteOffset / m_clusterSize);
        const uint32_t offsetInCluster = static_cast<uint32_t>(insertByteOffset % m_clusterSize);
        if (clusterIndex >= chain.size()) {
            throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Invalid cluster index for entry");
        }

        const uint64_t clusterOffset = clusterToOffset(chain[clusterIndex]) + offsetInCluster;
        std::vector<uint8_t> entry(ENTRY_SIZE, 0);
        const std::string shortName = createShortName(fileName);
        writeDirEntry(entry.data(), shortName, dirent.firstCluster, dirent.size, dirent.isDirectory,
                      dirent.mtime, dirent.atime, dirent.creationTime);
        if (!writeAt(clusterOffset, entry.data(), entry.size())) {
            throw IOException("writeDirectoryEntryToDisk", m_device->uri(), "Failed to write entry to disk");
        }
    }
}

void Fat32Volume::updateDirectoryEntryOnDisk(std::string_view path, const Dirent& dirent) {
    // For now, just write as new entry
    // A full implementation would find and update the existing entry
    writeDirectoryEntryToDisk(path, dirent);
}

bool Fat32Volume::findDirEntryLocation(std::string_view path, uint32_t& dirCluster, uint32_t& dirIndex,
                                       Dirent* outDirent) const {
    dirCluster = 0;
    dirIndex = INVALID_DIR_INDEX;
    const std::string resolved = resolvePath(path);
    if (resolved.empty()) {
        return false;
    }
    auto it = m_dirents.find(resolved);
    if (it == m_dirents.end() || it->second.isDirectory) {
        return false;
    }
    if (outDirent) {
        *outDirent = it->second;
    }

    const std::string parent = getParentPath(resolved);
    std::string fileName = getFileName(resolved);
    fileName = createShortName(fileName);

    auto pit = m_dirents.find(parent);
    if (pit == m_dirents.end() || !pit->second.isDirectory) {
        return false;
    }
    uint32_t parentCluster = pit->second.firstCluster;
    if (parent == "/" && parentCluster == 0) {
        parentCluster = m_rootCluster;
    }

    uint8_t fatName[11];
    memset(fatName, ' ', 11);
    std::string base = fileName;
    std::string ext;
    size_t dotPos = fileName.find('.');
    if (dotPos != std::string::npos && dotPos + 1 < fileName.size()) {
        base = fileName.substr(0, dotPos);
        ext = fileName.substr(dotPos + 1);
    }
    for (size_t i = 0; i < 8 && i < base.size(); ++i) {
        fatName[i] = static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(base[i])));
    }
    for (size_t i = 0; i < 3 && i < ext.size(); ++i) {
        fatName[8 + i] = static_cast<uint8_t>(std::toupper(static_cast<unsigned char>(ext[i])));
    }

    const std::vector<uint32_t> chain = readClusterChain(parentCluster);
    for (uint32_t cluster : chain) {
        std::vector<uint8_t> block(m_clusterSize);
        const uint64_t baseOff = clusterToOffset(cluster);
        if (!readAt(baseOff, block.data(), block.size())) {
            continue;
        }
        for (uint32_t pos = 0; pos + 32 <= m_clusterSize; pos += 32) {
            if (block[pos] == 0x00) {
                return false;
            }
            if (block[pos] == 0xE5 || block[pos] == 0x05 || block[pos + 11] == 0x0F) {
                continue;
            }
            bool matched = true;
            for (int i = 0; i < 11; ++i) {
                if (block[pos + i] != fatName[i]) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                dirCluster = cluster;
                dirIndex = pos / 32;
                return true;
            }
        }
    }
    return false;
}

void Fat32Volume::updateDirEntry(uint32_t dirCluster, uint32_t dirIndex, const Dirent& dirent) {
    if (dirCluster < 2 || dirIndex == INVALID_DIR_INDEX) {
        throw IOException("updateDirEntry", m_device->uri(), "Invalid directory entry location");
    }
    const uint64_t off = clusterToOffset(dirCluster) + static_cast<uint64_t>(dirIndex) * 32;
    if (off + 32 > m_device->size()) {
        throw IOException("updateDirEntry", m_device->uri(), "Directory entry out of range");
    }

    std::vector<uint8_t> oldEnt(32);
    if (!readAt(off, oldEnt.data(), oldEnt.size())) {
        throw IOException("updateDirEntry", m_device->uri(), "Failed to read old directory entry");
    }
    if (oldEnt[0] == 0x00 || oldEnt[0] == 0xE5 || oldEnt[11] == 0x0F) {
        throw IOException("updateDirEntry", m_device->uri(), "Target entry is invalid");
    }

    oldEnt[11] = dirent.isDirectory ? (oldEnt[11] | 0x10) : (oldEnt[11] & ~0x10);
    oldEnt[20] = static_cast<uint8_t>((dirent.firstCluster >> 16) & 0xFF);
    oldEnt[21] = static_cast<uint8_t>((dirent.firstCluster >> 24) & 0xFF);
    oldEnt[26] = static_cast<uint8_t>(dirent.firstCluster & 0xFF);
    oldEnt[27] = static_cast<uint8_t>((dirent.firstCluster >> 8) & 0xFF);
    oldEnt[28] = static_cast<uint8_t>(dirent.size & 0xFF);
    oldEnt[29] = static_cast<uint8_t>((dirent.size >> 8) & 0xFF);
    oldEnt[30] = static_cast<uint8_t>((dirent.size >> 16) & 0xFF);
    oldEnt[31] = static_cast<uint8_t>((dirent.size >> 24) & 0xFF);

    if (!writeAt(off, oldEnt.data(), oldEnt.size())) {
        throw IOException("updateDirEntry", m_device->uri(), "Failed to write directory entry");
    }
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
    
    bool found = false;
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
                if (!found) {
                    throw IOException("markDirectoryEntryAsDeleted", m_device->uri(),
                                     "Entry not found: " + std::string(name));
                }
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
                bool writeOk = writeAt(offset + pos, block.data() + pos, 32);
                if (!writeOk) {
                    throw IOException("markDirectoryEntryAsDeleted", m_device->uri(), "Failed to write deletion marker");
                }
                
                // Verify the write by reading back
                std::vector<uint8_t> verifyBlock(32);
                if (readAt(offset + pos, verifyBlock.data(), 32) && verifyBlock[0] != 0xE5) {
                    throw IOException("markDirectoryEntryAsDeleted", m_device->uri(),
                                     "Write verification failed - marker not persisted");
                }
                
                found = true;
                
                // Force OS to sync and drop cache for this region
                ::sync();
                // Note: Can't use posix_fadvise here because we don't have the file descriptor
                // The OS may re-cache old data, causing deleted entries to reappear
                
                return;  // Return immediately after successfully marking
            }
        }
    }
    
    if (!found) {
        throw IOException("markDirectoryEntryAsDeleted", m_device->uri(),
                         "Entry not found in directory: " + std::string(name));
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
            throw IOException("writeLFNEntries", m_device->uri(), "Invalid cluster index for LFN");
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
