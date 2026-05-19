#include "ExfatVolume.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

// Helper to get current time as exFAT timestamp
static uint32_t getExfatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    // exFAT timestamp format
    uint32_t timestamp = 0;
    timestamp |= (tm->tm_sec / 10) & 0x1F;                   // 10ms count (bits 0-4)
    timestamp |= (tm->tm_min & 0x3F) << 5;                   // Minutes (bits 5-10)
    timestamp |= (tm->tm_hour & 0x1F) << 11;                 // Hours (bits 11-15)
    timestamp |= (tm->tm_mday & 0x1F) << 16;                 // Day (bits 16-20)
    timestamp |= ((tm->tm_mon + 1) & 0x0F) << 21;            // Month (bits 21-24)
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
    auto data = readFile(src);
    if (!data.has_value()) {
        throw IOException("copyFile", src, "Source file is not readable");
    }

    // Write to destination
    writeFileUnchecked(destNormalized, *data);
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
    std::optional<std::vector<uint8_t>> data = readFileUnchecked(src);
    if (!data.has_value()) {
        throw IOException("moveFile", std::string(src), "Source file is not readable");
    }

    // Write to destination
    writeFileUnchecked(destNormalized, *data);

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
        throw IOException("renameFile", std::string(oldPath),
                          "Directory rename not yet implemented");
    }

    // Read file data
    auto data = readFile(oldPath);
    if (!data.has_value()) {
        throw IOException("renameFile", std::string(oldPath), "Source file is not readable");
    }

    // Write to new location
    writeFileUnchecked(newNormalized, *data);

    // Remove old entry
    removeFileThrowsUnchecked(oldPath);
}

uint32_t ExfatVolume::findFreeCluster() const {
    if (!m_clusterManager)
        return 0;
    return m_clusterManager->findFreeFromHint();
}

uint32_t ExfatVolume::allocateCluster(uint32_t prevCluster) {
    uint32_t cluster = findFreeCluster();
    if (cluster == 0) {
        return 0;
    }

    m_clusterManager->setAllocated(cluster, true);
    setFatEntry(cluster, 0xFFFFFFFF); // Mark as end of chain

    // Link to previous cluster if exists
    if (prevCluster != 0) {
        setFatEntry(prevCluster, cluster);
    }

    m_clusterManager->setAllocHint(cluster + 1);
    m_clusterManager->flushDirty();
    return cluster;
}

void ExfatVolume::freeClusterChain(uint32_t firstCluster) {
    uint32_t cluster = firstCluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = getFatEntry(cluster);

        m_clusterManager->setAllocated(cluster, false);
        setFatEntry(cluster, 0);

        if (next >= 0x0FFFFFF8 || next == cluster) {
            break;
        }
        cluster = next;
    }
    m_clusterManager->flushDirty();
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
    fileEntry[0] = 0x85; // File entry type
    fileEntry[1] = 0x00; // No special attributes
    uint32_t timestamp = getExfatTimestamp();
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 8) = timestamp;  // Create time
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 12) = timestamp; // Modify time
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 16) = timestamp; // Access time
    entries.push_back(fileEntry);

    // Stream entry (32 bytes)
    std::vector<uint8_t> streamEntry(32, 0);
    streamEntry[0] = 0xC0; // Stream entry type
    *reinterpret_cast<uint32_t*>(streamEntry.data() + 20) = firstCluster;
    *reinterpret_cast<uint64_t*>(streamEntry.data() + 24) = size;
    entries.push_back(streamEntry);

    // Filename entry (variable length, padded to 32 bytes)
    std::vector<uint8_t> nameEntry(32, 0);
    nameEntry[0] = 0xC1;            // Filename entry type
    nameEntry[1] = fileName.size(); // Name length

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

void ExfatVolume::updateFileEntry(std::string_view path, uint32_t firstCluster, uint64_t size) {
    // Simplified update: append a new directory entry set and update in-memory cache.
    // Current exFAT implementation always appends directory entries, so reusing createFileEntry
    // keeps behavior consistent and makes subsequent reads see the latest appended entry.
    createFileEntry(path, firstCluster, size);
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
    fileEntry[1] = 0x10; // Directory attribute
    uint32_t timestamp = getExfatTimestamp();
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 8) = timestamp;
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 12) = timestamp;
    *reinterpret_cast<uint32_t*>(fileEntry.data() + 16) = timestamp;
    entries.push_back(fileEntry);

    // Stream entry
    std::vector<uint8_t> streamEntry(32, 0);
    streamEntry[0] = 0xC0;
    *reinterpret_cast<uint32_t*>(streamEntry.data() + 20) = firstCluster;
    *reinterpret_cast<uint64_t*>(streamEntry.data() + 24) = 0; // Directories have size 0
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

void ExfatVolume::writeDirectoryEntries(uint32_t dirCluster,
                                        const std::vector<std::vector<uint8_t>>& entries) {
    // exFAT directory entries are stored as 32-byte records.
    // We must:
    // 1) Find empty slots (0x00 end-of-entries marker).
    // 2) If the directory is full in pages (no enough empty slots), reuse deleted slots (0xE5).
    // 3) If still full, extend the cluster chain and write into new zero-initialized clusters.

    constexpr size_t ENTRY_SLOT_SIZE = 32;
    if (entries.empty())
        return;

    size_t totalBytes = 0;
    for (const auto& e : entries) {
        totalBytes += e.size();
    }
    if (totalBytes % ENTRY_SLOT_SIZE != 0) {
        throw IOException("writeDirectoryEntries", "", "Invalid directory entry size");
    }
    const size_t requiredSlots = totalBytes / ENTRY_SLOT_SIZE;

    if (m_clusterSize < ENTRY_SLOT_SIZE || (m_clusterSize % ENTRY_SLOT_SIZE) != 0) {
        throw IOException("writeDirectoryEntries", "", "Invalid exFAT cluster size");
    }
    const size_t slotsPerCluster = m_clusterSize / ENTRY_SLOT_SIZE;

    std::vector<uint32_t> chain = readClusterChain(dirCluster);
    if (chain.empty()) {
        throw IOException("writeDirectoryEntries", "", "Directory has no clusters");
    }

    const std::vector<uint8_t> zeroCluster(m_clusterSize, 0);

    // ---- Step 1: Try to place into empty slots (0x00) ----
    // We treat the first 0x00 slot as the end-of-directory marker.
    uint64_t emptyStartOffset = 0;
    size_t emptyStartClusterIdx = 0;
    size_t emptyStartSlotIdx = 0;
    bool foundEmpty = false;

    // Track best deleted candidate (contiguous 0xE5 run) that ends before the empty marker.
    uint64_t deletedCandidateOffset = 0;
    size_t deletedRunLen = 0;
    uint64_t deletedRunStartOffset = 0;
    bool hasDeletedCandidate = false;

    auto readClusterBlock = [&](uint32_t cluster) -> std::vector<uint8_t> {
        std::vector<uint8_t> buf(m_clusterSize, 0);
        const uint64_t off = clusterToOffset(cluster);
        if (!readAt(off, buf.data(), buf.size())) {
            throw IOException("writeDirectoryEntries", m_device->name(),
                              "Failed to read directory cluster");
        }
        return buf;
    };

    for (size_t ci = 0; ci < chain.size(); ++ci) {
        std::vector<uint8_t> block = readClusterBlock(chain[ci]);
        for (size_t si = 0; si < slotsPerCluster; ++si) {
            const size_t slotOff = si * ENTRY_SLOT_SIZE;
            const uint8_t firstByte = block[slotOff];

            if (!foundEmpty) {
                if (firstByte == 0x00) {
                    foundEmpty = true;
                    emptyStartClusterIdx = ci;
                    emptyStartSlotIdx = si;
                    emptyStartOffset = clusterToOffset(chain[ci]) + slotOff;
                    break;
                }

                if (firstByte == 0xE5) {
                    if (deletedRunLen == 0) {
                        deletedRunStartOffset = clusterToOffset(chain[ci]) + slotOff;
                    }
                    deletedRunLen++;
                    if (!hasDeletedCandidate && deletedRunLen >= requiredSlots) {
                        deletedCandidateOffset = deletedRunStartOffset;
                        hasDeletedCandidate = true;
                        // We can keep scanning until empty marker to ensure candidate is truly
                        // before it, but we don't need to update it (keep earliest).
                    }
                } else {
                    deletedRunLen = 0;
                }
            }
        }

        if (foundEmpty)
            break;
    }

    // If empty marker exists, empty placement is preferred.
    if (foundEmpty) {
        // Count consecutive 0x00 slots starting from the empty marker.
        size_t emptyRunLen = 0;
        for (size_t ci = emptyStartClusterIdx; ci < chain.size(); ++ci) {
            std::vector<uint8_t> block = readClusterBlock(chain[ci]);
            const size_t startSi = (ci == emptyStartClusterIdx) ? emptyStartSlotIdx : 0;
            for (size_t si = startSi; si < slotsPerCluster; ++si) {
                const size_t slotOff = si * ENTRY_SLOT_SIZE;
                if (block[slotOff] != 0x00)
                    break;
                emptyRunLen++;
                if (emptyRunLen >= requiredSlots)
                    break;
            }
            if (emptyRunLen >= requiredSlots)
                break;
        }

        if (emptyRunLen >= requiredSlots) {
            uint64_t outOff = emptyStartOffset;
            for (const auto& entry : entries) {
                if (!writeAt(outOff, entry.data(), entry.size())) {
                    throw IOException("writeDirectoryEntries", "",
                                      "Failed to write directory entry");
                }
                outOff += entry.size();
            }
            return;
        }

        // Empty slots exist but not enough contiguous space.
        // Fallback to reuse deleted slots (before the empty marker).
        if (hasDeletedCandidate) {
            uint64_t outOff = deletedCandidateOffset;
            for (const auto& entry : entries) {
                if (!writeAt(outOff, entry.data(), entry.size())) {
                    throw IOException("writeDirectoryEntries", "",
                                      "Failed to write directory entry");
                }
                outOff += entry.size();
            }
            return;
        }

        // Still full: extend cluster chain.
        uint32_t prev = chain.back();
        size_t availableSlots = emptyRunLen;
        while (availableSlots < requiredSlots) {
            uint32_t newCluster = allocateCluster(prev);
            if (newCluster == 0) {
                throw IOException("writeDirectoryEntries", m_device->name(),
                                  "No space left on device");
            }
            if (!writeAt(clusterToOffset(newCluster), zeroCluster.data(), zeroCluster.size())) {
                throw IOException("writeDirectoryEntries", m_device->name(),
                                  "Failed to initialize directory cluster");
            }
            chain.push_back(newCluster);
            prev = newCluster;
            availableSlots += slotsPerCluster;
        }

        uint64_t outOff = emptyStartOffset;
        for (const auto& entry : entries) {
            if (!writeAt(outOff, entry.data(), entry.size())) {
                throw IOException("writeDirectoryEntries", "", "Failed to write directory entry");
            }
            outOff += entry.size();
        }
        return;
    }

    // ---- Step 2: No empty marker in existing chain -> reuse deleted slots ----
    deletedRunLen = 0;
    deletedRunStartOffset = 0;
    for (size_t ci = 0; ci < chain.size(); ++ci) {
        std::vector<uint8_t> block = readClusterBlock(chain[ci]);
        for (size_t si = 0; si < slotsPerCluster; ++si) {
            const size_t slotOff = si * ENTRY_SLOT_SIZE;
            const uint8_t firstByte = block[slotOff];

            if (firstByte == 0xE5) {
                if (deletedRunLen == 0) {
                    deletedRunStartOffset = clusterToOffset(chain[ci]) + slotOff;
                }
                deletedRunLen++;
                if (deletedRunLen >= requiredSlots) {
                    uint64_t outOff = deletedRunStartOffset;
                    for (const auto& entry : entries) {
                        if (!writeAt(outOff, entry.data(), entry.size())) {
                            throw IOException("writeDirectoryEntries", "",
                                              "Failed to write directory entry");
                        }
                        outOff += entry.size();
                    }
                    return;
                }
            } else {
                deletedRunLen = 0;
            }
        }
    }

    // ---- Step 3: Still full -> extend cluster chain and write into the new space ----
    const size_t neededClusters = (requiredSlots + slotsPerCluster - 1) / slotsPerCluster;
    uint32_t prev = chain.back();
    uint32_t firstNewCluster = 0;
    for (size_t i = 0; i < neededClusters; ++i) {
        uint32_t newCluster = allocateCluster(prev);
        if (newCluster == 0) {
            throw IOException("writeDirectoryEntries", m_device->name(), "No space left on device");
        }
        if (!writeAt(clusterToOffset(newCluster), zeroCluster.data(), zeroCluster.size())) {
            throw IOException("writeDirectoryEntries", m_device->name(),
                              "Failed to initialize directory cluster");
        }
        if (i == 0)
            firstNewCluster = newCluster;
        chain.push_back(newCluster);
        prev = newCluster;
    }

    uint64_t outOff = clusterToOffset(firstNewCluster);
    for (const auto& entry : entries) {
        if (!writeAt(outOff, entry.data(), entry.size())) {
            throw IOException("writeDirectoryEntries", "", "Failed to write directory entry");
        }
        outOff += entry.size();
    }
}
