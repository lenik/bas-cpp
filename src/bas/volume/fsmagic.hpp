#ifndef FSMAGIC_H
#define FSMAGIC_H

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * On-disk filesystem kinds recognized by magic-byte probing.
 * typeName in FsInfo carries the blkid-style string ("ext4", "vfat", ...).
 */
enum class FsType {
    None = 0,

    // Linux POSIX
    Ext2,
    Ext3,
    Ext4,
    Xfs,
    Btrfs,
    F2fs,
    Jfs,
    Reiserfs,
    Nilfs2,
    Erofs,
    Squashfs,
    Minix,

    // Microsoft / DOS
    Fat12,
    Fat16,
    Fat32,
    Exfat,
    Ntfs,

    // Apple
    Hfs,
    HfsPlus,
    Apfs,

    // Optical / archive-ish
    Iso9660,
    Udf,

    // Pools / volume managers / crypto / swap
    Zfs,
    Luks,
    Lvm2,
    LinuxRaid,
    Swap,

    Other,
};

enum class FsUsage {
    Unknown = 0,
    Filesystem,
    Raid,
    Crypto,
    Other,
};

/**
 * Common filesystem identity / geometry fields (blkid / df style).
 * Numeric sizes are in bytes. Unknown optional values stay 0 / empty.
 */
struct FsInfo {
    FsType type = FsType::None;
    FsUsage usage = FsUsage::Unknown;

    std::string typeName;  // "ext4", "vfat", "ntfs", ...
    std::string uuid;      // canonical id (UUID or FAT serial form)
    std::string uuidSub;   // secondary id (btrfs device uuid, etc.)
    std::string label;
    std::string version;
    std::string creator;   // OEM / system id
    std::string serial;    // volume serial when distinct from uuid

    uint32_t sectorSize = 0;  // physical/logical sector (often 512)
    uint32_t blockSize = 0;   // filesystem block size
    uint32_t clusterSize = 0; // FAT/exFAT cluster, etc.

    uint64_t size = 0;  // total capacity
    uint64_t free = 0;  // free/available when cheap to read
    uint64_t used = 0;  // used bytes when known

    bool clean = false;    // unmounted / consistent when known
    bool detected = false; // true if a signature matched
    uint64_t magicOffset = 0;

    // Back-compat aliases used by BlockDevice accessors.
    uint64_t capacity() const { return size; }
    uint64_t available() const { return free; }
};

std::string fsTypeToString(FsType t);
std::string fsUsageToString(FsUsage u);

/** Bytes recommended for probeFsMagic (covers btrfs@64K and zfs@128K). */
constexpr size_t kFsMagicProbeSize = 256 * 1024;

/**
 * Probe a buffer that starts at device/image offset 0.
 * @return FsInfo with detected=true on match; otherwise type=None.
 */
FsInfo probeFsMagic(const uint8_t* data, size_t len);

#endif // FSMAGIC_H
