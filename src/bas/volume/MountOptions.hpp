#ifndef MOUNTOPTIONS_H
#define MOUNTOPTIONS_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * Mount options for filesystem volumes.
 * Supports both file-backed and memory-backed images.
 */
struct MountOptions {
    // Image source
    std::string imagePath;                    // Path to image file (if file-backed)
    const uint8_t* memoryRegion = nullptr;   // Pointer to memory region (if memory-backed)
    size_t memorySize = 0;                    // Size of memory region
    
    // Mount flags
    bool readOnly = false;                    // Mount read-only
    bool noAutoMount = false;                 // Don't auto-mount
    
    // Performance options
    size_t bufferSize = 65536;               // I/O buffer size (default 64KB)
    bool writeThrough = false;                // Don't buffer writes
    
    // FAT32-specific options
    struct FAT32Options {
        bool useShortNames = true;            // Prefer 8.3 short names
        bool createLFN = false;               // Create LFN entries (not fully supported)
        uint8_t clusterSize = 0;              // 0 = auto-detect
    } fat32;
    
    // ext4-specific options
    struct Ext4Options {
        uint32_t uid = 0;                     // Default UID for created files
        uint32_t gid = 0;                     // Default GID for created files
        uint16_t fileMode = 0644;             // Default file permissions
        uint16_t dirMode = 0755;              // Default directory permissions
        bool journal = true;                  // Use journaling (if supported)
    } ext4;
    
    // Constructors
    MountOptions() = default;
    
    // Create file-backed mount options
    static MountOptions file(const std::string& path, bool readOnly = false) {
        MountOptions opts;
        opts.imagePath = path;
        opts.readOnly = readOnly;
        return opts;
    }
    
    // Create memory-backed mount options
    static MountOptions memory(const uint8_t* data, size_t size, bool readOnly = true) {
        MountOptions opts;
        opts.memoryRegion = data;
        opts.memorySize = size;
        opts.readOnly = readOnly;
        return opts;
    }
    
    // Check if memory-backed
    bool isMemoryBacked() const {
        return memoryRegion != nullptr && memorySize > 0;
    }
    
    // Check if file-backed
    bool isFileBacked() const {
        return !imagePath.empty();
    }
    
    // Get effective size
    size_t getSize() const {
        if (isMemoryBacked()) {
            return memorySize;
        }
        return 0;  // Will be determined from file
    }
};

#endif // MOUNTOPTIONS_H
