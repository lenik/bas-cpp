#include "fsmagic.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

uint16_t le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t le64(const uint8_t* p) {
    return static_cast<uint64_t>(le32(p)) | (static_cast<uint64_t>(le32(p + 4)) << 32);
}

uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

uint64_t be64(const uint8_t* p) {
    return (static_cast<uint64_t>(be32(p)) << 32) | be32(p + 4);
}

std::string formatUuid(const uint8_t* u) {
    static const char* hex = "0123456789abcdef";
    char out[37];
    int o = 0;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out[o++] = '-';
        out[o++] = hex[u[i] >> 4];
        out[o++] = hex[u[i] & 0xf];
    }
    out[o] = '\0';
    return std::string(out);
}

std::string cStringField(const uint8_t* p, size_t maxLen) {
    size_t n = 0;
    while (n < maxLen && p[n] != 0)
        ++n;
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\0'))
        --n;
    return std::string(reinterpret_cast<const char*>(p), n);
}

std::string fatSerial(uint32_t volId) {
    char serial[10];
    std::snprintf(serial, sizeof(serial), "%04X-%04X", (volId >> 16) & 0xffff, volId & 0xffff);
    return serial;
}

void markFs(FsInfo& info, FsType type, const char* name, uint64_t magicOff = 0) {
    info.type = type;
    info.typeName = name;
    info.usage = FsUsage::Filesystem;
    info.detected = true;
    info.magicOffset = magicOff;
}

bool probeLuks(const uint8_t* buf, size_t len, FsInfo& info) {
    if (len < 16)
        return false;
    if (std::memcmp(buf, "LUKS\xba\xbe", 6) != 0)
        return false;
    markFs(info, FsType::Luks, "crypto_LUKS", 0);
    info.usage = FsUsage::Crypto;
    info.version = std::to_string(buf[6]); // major
    // UUID at offset 168 (16 ASCII hex + dashes) in LUKS1
    if (len >= 168 + 40) {
        info.uuid = cStringField(buf + 168, 40);
    }
    return true;
}

bool probeLvm2(const uint8_t* buf, size_t len, FsInfo& info) {
    // LABELONE at sector 1 (offset 512)
    if (len < 512 + 8)
        return false;
    if (std::memcmp(buf + 512, "LABELONE", 8) != 0)
        return false;
    markFs(info, FsType::Lvm2, "LVM2_member", 512);
    info.usage = FsUsage::Other;
    if (len >= 512 + 0x28 + 32)
        info.uuid = cStringField(buf + 512 + 0x28, 32);
    return true;
}

bool probeLinuxRaid(const uint8_t* buf, size_t len, FsInfo& info) {
    // md 1.x superblock typically near end; 0.90 at end-64K. For offset-0 probe,
    // recognize IMSM / DDF poorly — also check "\xfc\x4e\x2b\xa9" rarely at start.
    // Common: at offset 4096 for 1.1: magic 0xa92b4efc le
    if (len < 4096 + 64)
        return false;
    if (le32(buf + 4096) == 0xa92b4efc) {
        markFs(info, FsType::LinuxRaid, "linux_raid_member", 4096);
        info.usage = FsUsage::Raid;
        if (len >= 4096 + 16 + 16)
            info.uuid = formatUuid(buf + 4096 + 16);
        return true;
    }
    return false;
}

bool probeSwap(const uint8_t* buf, size_t len, FsInfo& info) {
    if (len < 4096)
        return false;
    // Modern swap signature at page end - 10 chars ending at 4086..4096
    if (std::memcmp(buf + 4086, "SWAPSPACE2", 10) == 0 ||
        std::memcmp(buf + 4086, "SWAP-SPACE", 10) == 0) {
        markFs(info, FsType::Swap, "swap", 4086);
        info.usage = FsUsage::Other;
        // UUID at offset 0x40c (1036) for SWAPSPACE2
        if (len >= 0x41c && std::memcmp(buf + 4086, "SWAPSPACE2", 10) == 0)
            info.uuid = formatUuid(buf + 0x40c);
        return true;
    }
    return false;
}

bool probeXfs(const uint8_t* buf, size_t len, FsInfo& info) {
    if (len < 120 || std::memcmp(buf, "XFSB", 4) != 0)
        return false;
    markFs(info, FsType::Xfs, "xfs", 0);
    info.blockSize = be32(buf + 4);
    info.sectorSize = be16(buf + 102); // sb_sectsize
    const uint64_t dblocks = be64(buf + 8);
    info.size = dblocks * info.blockSize;
    info.uuid = formatUuid(buf + 32);
    info.label = cStringField(buf + 108, 12);
    info.clean = true;
    return true;
}

bool probeBtrfs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 0x10000;
    if (len < sb + 0x12b + 16)
        return false;
    const uint8_t* s = buf + sb;
    if (std::memcmp(s + 0x40, "_BHRfS_M", 8) != 0)
        return false;
    markFs(info, FsType::Btrfs, "btrfs", sb + 0x40);
    info.uuid = formatUuid(s + 0x20);       // fsid
    info.uuidSub = formatUuid(s + 0x10b);   // dev_item.uuid (super+0xc9+0x42)
    info.label = cStringField(s + 0x12b, 256);
    info.sectorSize = le32(s + 0x90);
    info.blockSize = le32(s + 0x94); // nodesize as "block"
    const uint64_t total = le64(s + 0x70);
    const uint64_t used = le64(s + 0x78);
    info.size = total;
    info.used = used;
    info.free = (total >= used) ? (total - used) : 0;
    info.clean = true;
    return true;
}

bool probeExt(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 1024;
    if (len < sb + 136)
        return false;
    const uint8_t* s = buf + sb;
    if (le16(s + 0x38) != 0xEF53)
        return false;

    const uint32_t featureCompat = le32(s + 0x5C);
    const uint32_t featureIncompat = le32(s + 0x60);
    FsType t = FsType::Ext2;
    const char* name = "ext2";
    if (featureIncompat & 0x40) {
        t = FsType::Ext4;
        name = "ext4";
    } else if (featureCompat & 0x4) {
        t = FsType::Ext3;
        name = "ext3";
    }
    markFs(info, t, name, sb + 0x38);

    const uint32_t logBlockSize = le32(s + 0x18);
    info.blockSize = 1024u << logBlockSize;
    info.sectorSize = 512;
    const uint64_t blocks = le32(s + 0x04);
    const uint64_t freeBlocks = le32(s + 0x0C);
    // ext4 64-bit counts if incompat 0x80
    uint64_t blocks64 = blocks;
    uint64_t free64 = freeBlocks;
    if ((featureIncompat & 0x80) && len >= sb + 0x158) {
        blocks64 |= static_cast<uint64_t>(le32(s + 0x150)) << 32;
        free64 |= static_cast<uint64_t>(le32(s + 0x158)) << 32;
    }
    info.size = blocks64 * info.blockSize;
    info.free = free64 * info.blockSize;
    info.used = (info.size >= info.free) ? (info.size - info.free) : 0;
    info.uuid = formatUuid(s + 0x68);
    info.label = cStringField(s + 0x78, 16);
    info.clean = (le16(s + 0x3A) == 1);
    const uint32_t rev = le32(s + 0x4C);
    info.version = std::to_string(rev);
    return true;
}

bool probeF2fs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 1024;
    if (len < sb + 0x7c)
        return false;
    const uint8_t* s = buf + sb;
    if (le32(s) != 0xF2F52010)
        return false;
    markFs(info, FsType::F2fs, "f2fs", sb);
    info.sectorSize = 512;
    info.blockSize = 4096;
    info.uuid = formatUuid(s + 0x6c);
    // volume_name is UTF-16LE; leave label empty on the fast path.
    if (len >= sb + 0x24) {
        const uint32_t blockCount = le32(s + 0x24);
        info.size = static_cast<uint64_t>(blockCount) * 4096;
    }
    info.clean = true;
    return true;
}

bool probeJfs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 32768;
    if (len < sb + 152)
        return false;
    const uint8_t* s = buf + sb;
    if (std::memcmp(s, "JFS1", 4) != 0)
        return false;
    markFs(info, FsType::Jfs, "jfs", sb);
    info.blockSize = le32(s + 0x18);
    info.uuid = formatUuid(s + 0x88);
    info.label = cStringField(s + 0x98, 16);
    const uint64_t blocks = le64(s + 0x08);
    info.size = blocks * info.blockSize;
    info.clean = true;
    return true;
}

bool probeReiserfs(const uint8_t* buf, size_t len, FsInfo& info) {
    // Superblock at 64KiB for reiserfs 3.6
    constexpr size_t sb = 0x10000;
    if (len < sb + 0x54)
        return false;
    const uint8_t* s = buf + sb;
    const char* mag = reinterpret_cast<const char*>(s + 0x34);
    if (std::memcmp(mag, "ReIsErFs", 8) != 0 && std::memcmp(mag, "ReIsEr2Fs", 9) != 0 &&
        std::memcmp(mag, "ReIsEr3Fs", 9) != 0)
        return false;
    markFs(info, FsType::Reiserfs, "reiserfs", sb + 0x34);
    info.blockSize = le16(s + 0x2c);
    info.uuid = formatUuid(s + 0x54);
    info.label = cStringField(s + 0x64, 16);
    const uint32_t blocks = le32(s + 0x00);
    info.size = static_cast<uint64_t>(blocks) * info.blockSize;
    info.clean = true;
    return true;
}

bool probeNilfs2(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 1024;
    if (len < sb + 0x28)
        return false;
    const uint8_t* s = buf + sb;
    if (le16(s + 0x06) != 0x3434)
        return false;
    markFs(info, FsType::Nilfs2, "nilfs2", sb + 0x06);
    info.blockSize = 1u << le32(s + 0x20);
    info.uuid = formatUuid(s + 0x268); // may be beyond small sb — guard
    if (len < sb + 0x278)
        info.uuid.clear();
    info.label = (len >= sb + 0x298) ? cStringField(s + 0x278, 80) : "";
    info.clean = true;
    return true;
}

bool probeErofs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 1024;
    if (len < sb + 16)
        return false;
    if (le32(buf + sb) != 0xE0F5E1E2)
        return false;
    markFs(info, FsType::Erofs, "erofs", sb);
    info.blockSize = 1u << (buf[sb + 12] & 0x1f);
    if (len >= sb + 64)
        info.uuid = formatUuid(buf + sb + 48);
    info.clean = true;
    return true;
}

bool probeSquashfs(const uint8_t* buf, size_t len, FsInfo& info) {
    if (len < 28)
        return false;
    const bool le = std::memcmp(buf, "hsqs", 4) == 0;
    const bool be = std::memcmp(buf, "sqsh", 4) == 0;
    if (!le && !be)
        return false;
    markFs(info, FsType::Squashfs, "squashfs", 0);
    info.blockSize = le ? le32(buf + 12) : be32(buf + 12);
    info.size = le ? le64(buf + 40) : be64(buf + 40); // bytes_used approx when len allows
    if (len < 48)
        info.size = 0;
    info.clean = true;
    return true;
}

bool probeMinix(const uint8_t* buf, size_t len, FsInfo& info) {
    // minix superblock at 1024
    if (len < 1024 + 20)
        return false;
    const uint16_t magic = le16(buf + 1024 + 0x10);
    if (magic != 0x137F && magic != 0x138F && magic != 0x2468 && magic != 0x2478)
        return false;
    markFs(info, FsType::Minix, "minix", 1024 + 0x10);
    info.blockSize = 1024;
    info.clean = true;
    return true;
}

bool probeFat(const uint8_t* buf, size_t len, FsInfo& info) {
    if (len < 512 || le16(buf + 510) != 0xAA55)
        return false;

    // exFAT: BPB bytes 11-12 are MustBeZero — check before FAT bytesPerSector validation.
    if (std::memcmp(buf + 3, "EXFAT   ", 8) == 0) {
        markFs(info, FsType::Exfat, "exfat", 3);
        info.sectorSize = 1u << buf[108];
        info.clusterSize = info.sectorSize * (1u << buf[109]);
        info.blockSize = info.sectorSize;
        info.creator = cStringField(buf + 3, 8);
        const uint32_t serial = le32(buf + 100);
        info.serial = fatSerial(serial);
        info.uuid = info.serial;
        // Cluster heap size / offset for capacity when present
        if (len >= 120) {
            const uint64_t clusterCount = le64(buf + 92);
            info.size = clusterCount * info.clusterSize;
        }
        info.clean = true;
        return true;
    }

    const uint16_t bps = le16(buf + 11);
    if (bps == 0 || (bps & (bps - 1)) != 0)
        return false;

    if (std::memcmp(buf + 3, "NTFS    ", 8) == 0) {
        markFs(info, FsType::Ntfs, "ntfs", 3);
        info.sectorSize = bps;
        info.blockSize = bps;
        info.clusterSize = static_cast<uint32_t>(bps) * buf[13];
        const uint64_t serial = le64(buf + 0x48);
        char sbuf[24];
        std::snprintf(sbuf, sizeof(sbuf), "%016llX", static_cast<unsigned long long>(serial));
        info.serial = sbuf;
        info.uuid = info.serial;
        info.size = le64(buf + 0x28) * bps;
        info.clean = true;
        return true;
    }

    const bool fat32 = std::memcmp(buf + 0x52, "FAT32   ", 8) == 0;
    const bool fat16 = std::memcmp(buf + 0x36, "FAT16   ", 8) == 0;
    const bool fat12 = std::memcmp(buf + 0x36, "FAT12   ", 8) == 0;
    if (!fat32 && !fat16 && !fat12)
        return false;

    FsType t = fat32 ? FsType::Fat32 : (fat16 ? FsType::Fat16 : FsType::Fat12);
    markFs(info, t, "vfat", fat32 ? 0x52 : 0x36);
    info.sectorSize = bps;
    info.blockSize = bps;
    info.clusterSize = static_cast<uint32_t>(bps) * buf[13];
    info.creator = cStringField(buf + 3, 8);

    uint32_t totalSectors = le16(buf + 19);
    if (totalSectors == 0)
        totalSectors = le32(buf + 32);
    info.size = static_cast<uint64_t>(totalSectors) * bps;

    uint32_t volId = 0;
    std::string label;
    if (fat32) {
        volId = le32(buf + 67);
        label = cStringField(buf + 71, 11);
    } else {
        volId = le32(buf + 39);
        label = cStringField(buf + 43, 11);
    }
    info.serial = fatSerial(volId);
    info.uuid = info.serial;
    info.label = label;
    info.clean = true;
    return true;
}

bool probeIso9660(const uint8_t* buf, size_t len, FsInfo& info) {
    // Primary volume descriptor at sector 16 (offset 32768)
    constexpr size_t off = 32768;
    if (len < off + 40)
        return false;
    if (buf[off] != 1 || std::memcmp(buf + off + 1, "CD001", 5) != 0)
        return false;
    markFs(info, FsType::Iso9660, "iso9660", off + 1);
    info.sectorSize = 2048;
    info.blockSize = 2048;
    info.label = cStringField(buf + off + 40, 32);
    info.creator = cStringField(buf + off + 318, 32); // publisher often; system id at 8
    if (len >= off + 8 + 32)
        info.creator = cStringField(buf + off + 8, 32);
    // volume space size at 80 (le32)
    info.size = static_cast<uint64_t>(le32(buf + off + 80)) * 2048;
    info.clean = true;
    return true;
}

bool probeUdf(const uint8_t* buf, size_t len, FsInfo& info) {
    // BEA01 / NSR0x / TEA01 volume recognition sequence starting ~32K
    constexpr size_t off = 32768;
    if (len < off + 6)
        return false;
    if (std::memcmp(buf + off + 1, "BEA01", 5) == 0 || std::memcmp(buf + off + 1, "NSR02", 5) == 0 ||
        std::memcmp(buf + off + 1, "NSR03", 5) == 0) {
        markFs(info, FsType::Udf, "udf", off + 1);
        info.sectorSize = 2048;
        info.clean = true;
        return true;
    }
    return false;
}

bool probeHfs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t sb = 1024;
    if (len < sb + 2)
        return false;
    const uint16_t sig = be16(buf + sb);
    if (sig == 0x482B || sig == 0x4858) { // H+ / HX
        markFs(info, FsType::HfsPlus, "hfsplus", sb);
        if (len >= sb + 112) {
            info.blockSize = be32(buf + sb + 40);
            const uint32_t totalBlocks = be32(buf + sb + 44);
            const uint32_t freeBlocks = be32(buf + sb + 48);
            info.size = static_cast<uint64_t>(totalBlocks) * info.blockSize;
            info.free = static_cast<uint64_t>(freeBlocks) * info.blockSize;
            info.used = (info.size >= info.free) ? info.size - info.free : 0;
            info.uuid = formatUuid(buf + sb + 104); // finder info / often not uuid — skip if zeros
            bool allZero = true;
            for (int i = 0; i < 16; ++i)
                if (buf[sb + 104 + i])
                    allZero = false;
            if (allZero)
                info.uuid.clear();
        }
        info.clean = true;
        return true;
    }
    if (sig == 0x4244) { // BD — HFS standard
        markFs(info, FsType::Hfs, "hfs", sb);
        info.clean = true;
        return true;
    }
    return false;
}

bool probeApfs(const uint8_t* buf, size_t len, FsInfo& info) {
    // NX superblock: magic NXSB at offset 32
    if (len < 64)
        return false;
    if (std::memcmp(buf + 32, "NXSB", 4) != 0)
        return false;
    markFs(info, FsType::Apfs, "apfs", 32);
    info.blockSize = le32(buf + 36);
    info.uuid = formatUuid(buf + 48); // container uuid
    info.clean = true;
    return true;
}

bool probeZfs(const uint8_t* buf, size_t len, FsInfo& info) {
    constexpr size_t uber = 0x20000;
    if (len < uber + 8)
        return false;
    const uint8_t* p = buf + uber;
    constexpr uint64_t kUberMagic = 0x00bab10cULL;
    if (le64(p) != kUberMagic && be64(p) != kUberMagic)
        return false;
    markFs(info, FsType::Zfs, "zfs_member", uber);
    info.usage = FsUsage::Other;
    info.clean = true;
    return true;
}

} // namespace

std::string fsTypeToString(FsType t) {
    switch (t) {
    case FsType::None:
        return "none";
    case FsType::Ext2:
        return "ext2";
    case FsType::Ext3:
        return "ext3";
    case FsType::Ext4:
        return "ext4";
    case FsType::Xfs:
        return "xfs";
    case FsType::Btrfs:
        return "btrfs";
    case FsType::F2fs:
        return "f2fs";
    case FsType::Jfs:
        return "jfs";
    case FsType::Reiserfs:
        return "reiserfs";
    case FsType::Nilfs2:
        return "nilfs2";
    case FsType::Erofs:
        return "erofs";
    case FsType::Squashfs:
        return "squashfs";
    case FsType::Minix:
        return "minix";
    case FsType::Fat12:
        return "fat12";
    case FsType::Fat16:
        return "fat16";
    case FsType::Fat32:
        return "fat32";
    case FsType::Exfat:
        return "exfat";
    case FsType::Ntfs:
        return "ntfs";
    case FsType::Hfs:
        return "hfs";
    case FsType::HfsPlus:
        return "hfsplus";
    case FsType::Apfs:
        return "apfs";
    case FsType::Iso9660:
        return "iso9660";
    case FsType::Udf:
        return "udf";
    case FsType::Zfs:
        return "zfs";
    case FsType::Luks:
        return "crypto_LUKS";
    case FsType::Lvm2:
        return "LVM2_member";
    case FsType::LinuxRaid:
        return "linux_raid_member";
    case FsType::Swap:
        return "swap";
    case FsType::Other:
        return "other";
    }
    return "unknown";
}

std::string fsUsageToString(FsUsage u) {
    switch (u) {
    case FsUsage::Unknown:
        return "unknown";
    case FsUsage::Filesystem:
        return "filesystem";
    case FsUsage::Raid:
        return "raid";
    case FsUsage::Crypto:
        return "crypto";
    case FsUsage::Other:
        return "other";
    }
    return "unknown";
}

FsInfo probeFsMagic(const uint8_t* data, size_t len) {
    FsInfo info{};
    if (!data || len == 0)
        return info;

    // Order: crypto/raid/swap and distinctive magics before ambiguous FAT boot sectors.
    using ProbeFn = bool (*)(const uint8_t*, size_t, FsInfo&);
    static const ProbeFn kProbes[] = {
        probeLuks,      //
        probeLvm2,      //
        probeLinuxRaid, //
        probeSwap,      //
        probeXfs,       //
        probeBtrfs,     //
        probeZfs,       //
        probeApfs,      //
        probeExt,       //
        probeF2fs,      //
        probeErofs,     //
        probeNilfs2,    //
        probeJfs,       //
        probeReiserfs,  //
        probeSquashfs,  //
        probeMinix,     //
        probeHfs,       //
        probeIso9660,   //
        probeUdf,       //
        probeFat,       // fat/exfat/ntfs last (boot-sector style)
    };

    for (ProbeFn fn : kProbes) {
        FsInfo cand{};
        if (fn(data, len, cand)) {
            if (cand.typeName.empty())
                cand.typeName = fsTypeToString(cand.type);
            return cand;
        }
    }
    return info;
}
