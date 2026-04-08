#include "ExfatVolume.hpp"
#include "../../io/IOException.hpp"
#include "../../io/PrintStream.hpp"
#include "../../io/StringReader.hpp"
#include "ExfatFileInputStream.hpp"
#include "ExfatFileOutputStream.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <unordered_set>

// exFAT boot sector structure
#pragma pack(push, 1)
struct ExfatBootSector {
    uint8_t jmpBoot[3];
    char oemName[8];
    uint8_t reserved[53];
    uint64_t partitionOffset;
    uint64_t volumeLength;
    uint32_t fatOffset;
    uint32_t fatLength;
    uint32_t clusterHeapOffset;
    uint32_t clusterCount;
    uint32_t firstClusterOfRootDirectory;
    uint32_t volumeSerialNumber;
    uint8_t fileSystemRevision[3];
    uint16_t volumeFlags;
    uint8_t bytesPerSectorShift;
    uint8_t sectorsPerClusterShift;
    uint8_t numberOfFats;
    uint8_t driveSelect;
    uint8_t percentInUse;
    uint8_t reserved2[7];
    uint8_t bootCode[390];
    uint16_t bootSignature;
};
#pragma pack(pop)

// exFAT directory entry types
enum ExfatEntryType : uint8_t {
    ENTRY_TYPE_ALLOC_BITMAP = 0x81,
    ENTRY_TYPE_UPCASE_TABLE = 0x82,
    ENTRY_TYPE_VOLUME_LABEL = 0x83,
    ENTRY_TYPE_FILE = 0x85,
    ENTRY_TYPE_STREAM = 0xC0,
    ENTRY_TYPE_FILENAME = 0xC1,
    ENTRY_TYPE_BITMAP = 0x81,
};

void ExfatVolume::Dirent::copyTo(DirNode& node) const {
    if (isDirectory) {
        node.setDirectoryClear();
    } else {
        node.setRegularClear();
    }
    node.size = static_cast<size_t>(size);
    node.epochSeconds(mtime);
    node.accessTime(std::chrono::system_clock::from_time_t(atime));
    node.creationTime(std::chrono::system_clock::from_time_t(ctime));
    node.mode = static_cast<int>((isDirectory ? S_IFDIR : S_IFREG) | 0755);
}

ExfatVolume::ExfatVolume(std::shared_ptr<BlockDevice> device, const ExfatOptions& options)
    : m_device(device)
    , m_options(options)
{
    parseBootSector();
    buildIndex();
}

std::string ExfatVolume::getDefaultLabel() const { return "exFAT Volume"; }

void ExfatVolume::parseBootSector() {
    if (m_device->size() < 512) {
        throw IOException("ExfatVolume", m_device->name(), "Invalid image: too small");
    }
    
    std::vector<uint8_t> boot(512);
    if (!m_device->read(0, boot.data(), boot.size())) {
        throw IOException("ExfatVolume", m_device->name(), "Failed to read boot sector");
    }
    
    const ExfatBootSector* bs = reinterpret_cast<const ExfatBootSector*>(boot.data());
    
    // Validate boot signature
    if (bs->bootSignature != 0xAA55) {
        throw IOException("ExfatVolume", m_device->name(), "Invalid boot signature");
    }
    
    m_bytesPerSector = 1 << bs->bytesPerSectorShift;
    m_sectorsPerCluster = 1 << bs->sectorsPerClusterShift;
    m_clusterSize = m_bytesPerSector * m_sectorsPerCluster;
    m_fatOffset = bs->fatOffset;
    m_fatLength = bs->fatLength;
    m_clusterHeapOffset = bs->clusterHeapOffset;
    m_clusterCount = bs->clusterCount;
    m_rootCluster = bs->firstClusterOfRootDirectory;
    m_volumeSerial = bs->volumeSerialNumber;
    
    if (m_bytesPerSector == 0 || m_sectorsPerCluster == 0) {
        throw IOException("ExfatVolume", m_device->name(), "Invalid exFAT boot parameters");
    }
    
    // Parse allocation bitmap and upcase table
    parseBitmap();
    parseUpcaseTable();
}

void ExfatVolume::parseBitmap() {
    // Read first cluster of root to find bitmap entry
    uint64_t rootOffset = clusterToOffset(m_rootCluster);
    std::vector<uint8_t> cluster(m_bytesPerSector * m_sectorsPerCluster);
    
    if (!m_device->read(rootOffset, cluster.data(), cluster.size())) {
        throw IOException("ExfatVolume", m_device->name(), "Failed to read root cluster");
    }
    
    // Scan for bitmap entry
    for (size_t i = 0; i + 32 <= cluster.size(); i += 32) {
        const uint8_t* entry = cluster.data() + i;
        if (entry[0] == ENTRY_TYPE_ALLOC_BITMAP) {
            m_bitmapCluster = *reinterpret_cast<const uint32_t*>(entry + 20);
            m_bitmapLength = *reinterpret_cast<const uint32_t*>(entry + 24);
            break;
        }
        if (entry[0] == 0x00) break;  // End of entries
    }
}

void ExfatVolume::parseUpcaseTable() {
    // Read upcase table from root directory
    uint64_t rootOffset = clusterToOffset(m_rootCluster);
    std::vector<uint8_t> cluster(m_bytesPerSector * m_sectorsPerCluster);
    
    if (!m_device->read(rootOffset, cluster.data(), cluster.size())) {
        return;  // Upcase table is optional
    }
    
    // Scan for upcase table entry
    for (size_t i = 0; i + 32 <= cluster.size(); i += 32) {
        const uint8_t* entry = cluster.data() + i;
        if (entry[0] == ENTRY_TYPE_UPCASE_TABLE) {
            m_upcaseCluster = *reinterpret_cast<const uint32_t*>(entry + 20);
            m_upcaseLength = *reinterpret_cast<const uint32_t*>(entry + 24);
            
            // Load upcase table
            uint64_t upcaseOffset = clusterToOffset(m_upcaseCluster);
            std::vector<uint8_t> tableData(m_upcaseLength);
            if (m_device->read(upcaseOffset, tableData.data(), tableData.size())) {
                m_upcaseTable.resize(m_upcaseLength / 2);
                for (size_t j = 0; j < m_upcaseTable.size(); j++) {
                    m_upcaseTable[j] = tableData[j * 2] | (tableData[j * 2 + 1] << 8);
                }
            }
            break;
        }
        if (entry[0] == 0x00) break;
    }
}

void ExfatVolume::buildIndex() {
    m_dirents.clear();
    m_rtnodes.clear();
    m_chainCache.clear();
    
    // Add root directory
    Dirent root;
    root.isDirectory = true;
    root.firstCluster = m_rootCluster;
    root.size = 0;
    root.childrenParsed = false;
    m_dirents["/"] = root;
}

uint64_t ExfatVolume::clusterToOffset(uint32_t cluster) const {
    if (cluster < 2) {
        throw IOException("ExfatVolume", m_device->name(), "Invalid cluster number");
    }
    const uint64_t dataClusterIndex = static_cast<uint64_t>(cluster) - 2;
    return static_cast<uint64_t>(m_clusterHeapOffset) * m_bytesPerSector + 
           (dataClusterIndex * m_bytesPerSector * m_sectorsPerCluster);
}

uint32_t ExfatVolume::getFatEntry(uint32_t cluster) const {
    const uint64_t fatOffset = static_cast<uint64_t>(m_fatOffset) * m_bytesPerSector + 
                               static_cast<uint64_t>(cluster) * 4;
    uint8_t b[4] = {0, 0, 0, 0};
    if (!m_device->read(fatOffset, b, sizeof(b))) {
        throw IOException("ExfatVolume", m_device->name(), "Failed to read FAT entry");
    }
    return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) & 0x0FFFFFFF;
}

void ExfatVolume::setFatEntry(uint32_t cluster, uint32_t value) {
    const uint64_t fatOffset = static_cast<uint64_t>(m_fatOffset) * m_bytesPerSector + 
                               static_cast<uint64_t>(cluster) * 4;
    uint8_t b[4];
    b[0] = value & 0xFF;
    b[1] = (value >> 8) & 0xFF;
    b[2] = (value >> 16) & 0xFF;
    b[3] = (value >> 24) & 0xFF;
    m_device->write(fatOffset, b, sizeof(b));
}

std::vector<uint32_t> ExfatVolume::readClusterChain(uint32_t firstCluster) const {
    std::vector<uint32_t> chain;
    std::unordered_set<uint32_t> seen;
    
    uint32_t cluster = firstCluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        if (!seen.insert(cluster).second) {
            throw IOException("ExfatVolume", m_device->name(), "Detected FAT loop");
        }
        chain.push_back(cluster);
        cluster = getFatEntry(cluster);
    }
    return chain;
}

bool ExfatVolume::readAt(uint64_t offset, uint8_t* dst, size_t len) const {
    if (!dst || len == 0 || offset + len > m_device->size()) {
        return false;
    }
    return m_device->read(offset, dst, len);
}

bool ExfatVolume::writeAt(uint64_t offset, const uint8_t* src, size_t len) {
    if (!src || len == 0 || offset + len > m_device->size()) {
        return false;
    }
    return m_device->write(offset, src, len);
}

bool ExfatVolume::exists(std::string_view path) const {
    return ensurePathIndexed(path);
}

bool ExfatVolume::isFile(std::string_view path) const {
    if (!ensurePathIndexed(path)) return false;
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    return it != m_dirents.end() && !it->second.isDirectory;
}

bool ExfatVolume::isDirectory(std::string_view path) const {
    if (!ensurePathIndexed(path)) return false;
    const std::string normalized = normalizeArg(path);
    auto it = m_dirents.find(normalized);
    return it != m_dirents.end() && it->second.isDirectory;
}

bool ExfatVolume::stat(std::string_view path, DirNode* status) const {
    if (!status) throw std::invalid_argument("ExfatVolume::stat: status is null");
    
    const std::string normalized = normalizeArg(path);
    if (!ensurePathIndexed(normalized)) return false;
    
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end()) return false;
    
    it->second.copyTo(*status);
    status->name = getFileName(normalized);
    return true;
}

std::string ExfatVolume::normalizeArg(std::string_view path) const {
    std::string result(path);
    if (result.empty()) return "/";
    
    // Normalize slashes
    std::string normalized;
    bool lastWasSlash = false;
    for (char c : result) {
        if (c == '/') {
            if (!lastWasSlash) {
                normalized += c;
                lastWasSlash = true;
            }
        } else {
            normalized += c;
            lastWasSlash = false;
        }
    }
    
    if (normalized.empty()) return "/";
    if (normalized != "/" && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

std::string ExfatVolume::getParentPath(std::string_view path) const {
    std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) return "/";
    
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) return "/";
    
    return normalized.substr(0, lastSlash);
}

std::string ExfatVolume::getFileName(std::string_view path) const {
    std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) return "";
    
    size_t lastSlash = normalized.find_last_of('/');
    if (lastSlash == std::string::npos) return normalized;
    
    return normalized.substr(lastSlash + 1);
}

bool ExfatVolume::ensurePathIndexed(std::string_view path) const {
    std::string normalized = normalizeArg(path);
    if (normalized.empty() || normalized == "/") return true;
    if (m_dirents.find(normalized) != m_dirents.end()) return true;
    
    // Walk path from root
    std::string current = "/";
    size_t pos = 1;
    
    while (pos < normalized.size()) {
        size_t slash = normalized.find('/', pos);
        std::string part = (slash == std::string::npos) ? 
                          normalized.substr(pos) : 
                          normalized.substr(pos, slash - pos);
        
        if (part.empty() || part == ".") {
            if (slash == std::string::npos) break;
            pos = slash + 1;
            continue;
        }
        
        if (!ensureDirectoryParsed(current)) return false;
        
        auto it = m_dirents.find(current);
        if (it == m_dirents.end()) return false;
        
        auto childIt = it->second.children.find(part);
        if (childIt == it->second.children.end()) return false;
        
        current = (current == "/") ? ("/" + part) : (current + "/" + part);
        
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    
    return m_dirents.find(normalized) != m_dirents.end();
}

bool ExfatVolume::ensureDirectoryParsed(std::string_view dirPath) const {
    const std::string normalized = normalizeArg(dirPath);
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end() || !it->second.isDirectory) return false;
    if (it->second.childrenParsed) return true;
    
    parseDirectoryCluster(normalized, it->second.firstCluster);
    return true;
}

void ExfatVolume::parseDirectoryCluster(std::string_view dirPath, uint32_t firstCluster) const {
    auto itParent = m_dirents.find(std::string(dirPath));
    if (itParent == m_dirents.end()) return;
    
    Dirent& parent = itParent->second;
    parent.children.clear();
    
    if (firstCluster < 2) {
        parent.childrenParsed = true;
        return;
    }
    
    std::string currentName;
    Dirent currentDirent;
    bool hasCurrent = false;
    
    for (uint32_t cluster : readClusterChain(firstCluster)) {
        uint64_t offset = clusterToOffset(cluster);
        std::vector<uint8_t> block(m_bytesPerSector * m_sectorsPerCluster);
        
        if (!m_device->read(offset, block.data(), block.size())) continue;
        
        for (size_t pos = 0; pos + 32 <= block.size(); pos += 32) {
            const uint8_t* entry = block.data() + pos;
            
            if (entry[0] == 0x00) {
                // End of entries
                if (hasCurrent) {
                    std::string fullPath = (dirPath == "/") ? 
                                          ("/" + currentName) : 
                                          (std::string(dirPath) + "/" + currentName);
                    m_dirents[fullPath] = currentDirent;
                    parent.children[currentName] = &m_dirents[fullPath];
                }
                parent.childrenParsed = true;
                return;
            }
            
            if (entry[0] == 0xE5) continue;  // Deleted entry
            
            uint8_t type = entry[0] & 0x7F;
            
            if (type == ENTRY_TYPE_FILE) {
                // Save previous entry if any
                if (hasCurrent) {
                    std::string fullPath = (dirPath == "/") ? 
                                          ("/" + currentName) : 
                                          (std::string(dirPath) + "/" + currentName);
                    m_dirents[fullPath] = currentDirent;
                    parent.children[currentName] = &m_dirents[fullPath];
                }
                
                // Start new file entry
                hasCurrent = true;
                currentName.clear();
                currentDirent = Dirent();
                currentDirent.isDirectory = (entry[1] & 0x10) != 0;  // Directory flag
                
                // Get timestamps
                uint32_t createTime = *reinterpret_cast<const uint32_t*>(entry + 8);
                uint32_t modTime = *reinterpret_cast<const uint32_t*>(entry + 12);
                uint32_t accessTime = *reinterpret_cast<const uint32_t*>(entry + 16);
                
                // Convert exFAT timestamps (simplified)
                currentDirent.ctime = (createTime >> 16) & 0xFFFF;
                currentDirent.mtime = (modTime >> 16) & 0xFFFF;
                currentDirent.atime = accessTime;
                
            } else if (type == ENTRY_TYPE_STREAM && hasCurrent) {
                currentDirent.firstCluster = *reinterpret_cast<const uint32_t*>(entry + 20);
                currentDirent.size = *reinterpret_cast<const uint64_t*>(entry + 24);
                
            } else if (type == ENTRY_TYPE_FILENAME && hasCurrent) {
                // Read filename (UTF-16)
                uint8_t nameLength = entry[1];
                if (currentName.empty()) {
                    for (int i = 0; i < nameLength && i < 30; i++) {
                        uint16_t ch = entry[2 + i * 2] | (entry[3 + i * 2] << 8);
                        if (ch != 0) currentName += static_cast<char>(ch);
                    }
                }
            }
        }
    }
    
    // Save last entry
    if (hasCurrent) {
        std::string fullPath = (dirPath == "/") ? 
                              ("/" + currentName) : 
                              (std::string(dirPath) + "/" + currentName);
        m_dirents[fullPath] = currentDirent;
        parent.children[currentName] = &m_dirents[fullPath];
    }
    
    parent.childrenParsed = true;
}

uint16_t ExfatVolume::upcaseChar(uint16_t ch) const {
    if (m_upcaseTable.empty() || ch >= m_upcaseTable.size()) {
        // Simple ASCII uppercase
        if (ch >= 'a' && ch <= 'z') return ch - ('a' - 'A');
        return ch;
    }
    return m_upcaseTable[ch];
}

uint32_t ExfatVolume::computeChecksum(const uint8_t* data, size_t len, uint32_t initial) const {
    uint32_t checksum = initial;
    for (size_t i = 0; i < len; i++) {
        checksum = ((checksum & 1) << 31) | (checksum >> 1);
        checksum += data[i];
    }
    return checksum;
}

void ExfatVolume::invalidateCache(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    
    auto fit = m_dirents.find(normalized);
    if (fit != m_dirents.end()) {
        if (!fit->second.isDirectory) {
            uint32_t ino = fit->second.firstCluster;
            m_chainCache.erase(ino);
        }
        m_dirents.erase(fit);
    }
}
