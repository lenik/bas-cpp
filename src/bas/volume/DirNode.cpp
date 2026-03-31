#include "DirNode.hpp"

#include <chrono>
#include <ctime>

static auto timestamp(struct statx_timestamp tv) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(tv.tv_sec) +
                                                 std::chrono::nanoseconds(tv.tv_nsec));
}

static auto tv_sec(time_t tv_sec) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(tv_sec));
}

#if defined(__linux__)
void DirNode::assign(const struct statx& sx) {
    type = S_ISDIR(sx.stx_mode) ? FileType::Directory : FileType::Regular;
    if (S_ISREG(sx.stx_mode))
        size = sx.stx_size;
    else
        size = 0;
    inode = sx.stx_ino;
    modifiedTime(timestamp(sx.stx_mtime));

    accessTime(timestamp(sx.stx_atime));
    if (sx.stx_mask & STATX_BTIME)
        creationTime(timestamp(sx.stx_btime));
    else
        creationTime(timestamp(sx.stx_mtime));

    mode = sx.stx_mode;
    uid = sx.stx_uid;
    gid = sx.stx_gid;
    nlink = sx.stx_nlink;
    dev = sx.stx_dev_major << 8 | sx.stx_dev_minor;
    blksize = sx.stx_blksize;
    blocks = sx.stx_blocks;
}
#endif

void DirNode::assign(const struct stat& sb) {
    type = S_ISDIR(sb.st_mode) ? FileType::Directory : FileType::Regular;
    if (S_ISREG(sb.st_mode))
        size = static_cast<uint64_t>(sb.st_size);
    else
        size = 0;
    inode = sb.st_ino;
    modifiedTime(tv_sec(sb.st_mtime));

    accessTime(tv_sec(sb.st_atime));
    creationTime(tv_sec(sb.st_mtime));

    mode = sb.st_mode;
    uid = sb.st_uid;
    gid = sb.st_gid;
    nlink = sb.st_nlink;
    dev = sb.st_dev;
    blksize = sb.st_blksize;
    blocks = sb.st_blocks;
}
