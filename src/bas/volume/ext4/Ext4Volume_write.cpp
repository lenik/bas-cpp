#include "Ext4Volume.hpp"

#include "../../io/IOException.hpp"

#include <ext2fs/ext2_fs.h>
#include <ext2fs/ext2fs.h>

#include <cstdint>
#include <cstring>
#include <unistd.h>


void Ext4Volume::writeFileUnchecked(std::string_view path, const ByteArray& data) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("writeFile", std::string(path), "Cannot write to root directory");
    }

    // Check permissions
    const std::string parent = normalized.substr(0, normalized.find_last_of('/'));
    uint32_t parentIno = resolveParentInode(normalized);
    
    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0, 
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("writeFile", std::string(path), "ext2fs_open failed");
    }
    
    // Read bitmaps and initialize filesystem state
    rc = ext2fs_read_bitmaps(fs);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("writeFile", std::string(path), "Failed to read bitmaps");
    }

    struct ext2_inode inode{};
    ext2_ino_t ino = 0;
    
    // Try to find existing inode
    Inode existingNode;
    if (resolveNode(normalized, &existingNode)) {
        ino = static_cast<ext2_ino_t>(existingNode.ino);
        if (ext2fs_read_inode(fs, ino, &inode) != 0) {
            ext2fs_close(fs);
            throw IOException("writeFile", std::string(path), "Failed to read inode");
        }
    } else {
        // Create new file
        rc = ext2fs_new_inode(fs, parentIno, 0100644, nullptr, &ino);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("writeFile", std::string(path), "Failed to allocate inode");
        }
        
        memset(&inode, 0, sizeof(inode));
        inode.i_mode = 0100644; // Regular file, rw-r--r--
        inode.i_uid = m_contextUid;
        inode.i_gid = m_contextGid;
        inode.i_atime = inode.i_mtime = inode.i_ctime = time(nullptr);
    }

    // Write file data
    ext2_file_t file;
    rc = ext2fs_file_open(fs, ino, EXT2_FILE_WRITE | EXT2_FILE_CREATE, &file);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("writeFile", std::string(path), "Failed to open file for write");
    }

    if (!data.empty()) {
        unsigned int written;
        rc = ext2fs_file_write(file, data.data(), data.size(), &written);
        if (rc || written != data.size()) {
            ext2fs_file_close(file);
            ext2fs_close(fs);
            throw IOException("writeFile", std::string(path), "Failed to write data");
        }
    }

    ext2fs_file_close(file);

    // Re-read inode to get updated metadata (ext2fs updates it during write)
    rc = ext2fs_read_inode(fs, ino, &inode);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("writeFile", std::string(path), "Failed to re-read inode");
    }
    
    // Ensure size is correct
    if (inode.i_size != data.size()) {
        inode.i_size = data.size();
        rc = ext2fs_write_inode(fs, ino, &inode);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("writeFile", std::string(path), "Failed to update inode size");
        }
    }

    // Create directory entry if file doesn't exist
    if (!existingNode.ino) {
        std::string baseName = getBaseName(normalized);
        rc = ext2fs_link(fs, parentIno, baseName.c_str(), ino, 0100644);
        if (rc == EXT2_ET_DIR_NO_SPACE) {
            rc = ext2fs_expand_dir(fs, parentIno);
            if (rc) {
                ext2fs_close(fs);
                throw IOException("writeFile", std::string(path), "Failed to expand directory");
            }
            rc = ext2fs_link(fs, parentIno, baseName.c_str(), ino, 0100644);
        }
        if (rc) {
            ext2fs_close(fs);
            throw IOException("writeFile", std::string(path), "Failed to create directory entry: " + std::string(error_message(rc)));
        }
    }
    
    // Flush filesystem to ensure changes are written to disk
    rc = ext2fs_flush(fs);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("writeFile", std::string(path), "Failed to flush filesystem");
    }

    ext2fs_close(fs);
    
    // Force OS to sync the file to disk
    ::sync();
    
    // Manually add the new file to cache since buildIndex() doesn't scan directories
    m_files[normalized] = ino;
    
    // Add inode to cache
    Inode newInode;
    newInode.isDirectory = false;
    newInode.size = data.size();
    newInode.ino = ino;
    newInode.mode = 0100644;
    newInode.uid = m_contextUid;
    newInode.gid = m_contextGid;
    newInode.atime = newInode.mtime = newInode.ctime = time(nullptr);
    m_nodes[ino] = newInode;
    
    // Clear cached file content
    m_mem.erase(ino);
    
    // Clear parent directory cache to force re-scan
    const std::string parentPath = normalized.substr(0, normalized.find_last_of('/'));
    if (!parentPath.empty()) {
        auto pit = m_files.find(parentPath);
        if (pit != m_files.end()) {
            m_rtnodes.erase(pit->second);
        }
    } else {
        m_rtnodes.erase(2); // Root
    }
}

void Ext4Volume::createDirectoryThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("createDirectory", std::string(path), "Cannot create root directory");
    }

    // Check if already exists
    Inode existingNode;
    if (resolveNode(normalized, &existingNode)) {
        throw IOException("createDirectory", std::string(path), "Path already exists");
    }

    // Get parent inode
    uint32_t parentIno = resolveParentInode(normalized);

    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0,
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("createDirectory", std::string(path), "ext2fs_open failed");
    }
    
    // Read bitmaps and initialize filesystem state
    rc = ext2fs_read_bitmaps(fs);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("createDirectory", std::string(path), "Failed to read bitmaps");
    }

    // Allocate new inode for directory
    ext2_ino_t ino;
    rc = ext2fs_new_inode(fs, parentIno, 040755, nullptr, &ino);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("createDirectory", std::string(path), "Failed to allocate inode");
    }

    // Initialize directory inode
    struct ext2_inode inode{};
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = 040755; // Directory, rwxr-xr-x
    inode.i_uid = m_contextUid;
    inode.i_gid = m_contextGid;
    const time_t now = time(nullptr);
    inode.i_atime = inode.i_mtime = inode.i_ctime = now;

    // Write inode
    rc = ext2fs_write_inode(fs, ino, &inode);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("createDirectory", std::string(path), "Failed to write inode");
    }

    // Create directory entry in parent
    std::string baseName = getBaseName(normalized);
    rc = ext2fs_link(fs, parentIno, baseName.c_str(), ino, 040755);
    if (rc == EXT2_ET_DIR_NO_SPACE) {
        rc = ext2fs_expand_dir(fs, parentIno);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("createDirectory", std::string(path), "Failed to expand directory");
        }
        rc = ext2fs_link(fs, parentIno, baseName.c_str(), ino, 040755);
    }
    if (rc) {
        ext2fs_close(fs);
        throw IOException("createDirectory", std::string(path), "Failed to create directory entry: " + std::string(error_message(rc)));
    }
    
    // Flush to ensure directory entry is written
    rc = ext2fs_flush(fs);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("createDirectory", std::string(path), "Failed to flush filesystem");
    }

    ext2fs_close(fs);

    // Manually add directory to cache
    m_files[normalized] = ino;
    
    Inode dirInode;
    dirInode.isDirectory = true;
    dirInode.size = 0;
    dirInode.ino = ino;
    dirInode.mode = 040755;
    dirInode.uid = m_contextUid;
    dirInode.gid = m_contextGid;
    dirInode.atime = dirInode.mtime = dirInode.ctime = time(nullptr);
    m_nodes[ino] = dirInode;
    
    // Clear parent directory cache to force re-scan
    const std::string parentPath = normalized.substr(0, normalized.find_last_of('/'));
    if (!parentPath.empty()) {
        auto pit = m_files.find(parentPath);
        if (pit != m_files.end()) {
            m_rtnodes.erase(pit->second);
        }
    } else {
        m_rtnodes.erase(2);
    }
    
    // Also clear our own runtime node cache entry so it gets re-scanned
    m_rtnodes.erase(ino);
}

void Ext4Volume::removeDirectoryThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (normalized == "/" || normalized.empty()) {
        throw IOException("removeDirectory", std::string(path), "Cannot remove root directory");
    }

    // Check if directory exists
    Inode node;
    if (!resolveNode(normalized, &node) || !node.isDirectory) {
        throw IOException("removeDirectory", std::string(path), "Directory does not exist");
    }

    // Check if directory is empty
    const auto& rtnode = getDirectoryEntries(node.ino);
    if (!rtnode->children.empty()) {
        throw IOException("removeDirectory", std::string(path), "Directory is not empty");
    }

    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0,
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("removeDirectory", std::string(path), "ext2fs_open failed");
    }

    // Remove directory entry from parent
    const std::string parent = normalized.substr(0, normalized.find_last_of('/'));
    uint32_t parentIno = resolveParentInode(normalized);
    std::string baseName = getBaseName(normalized);

    rc = ext2fs_unlink(fs, parentIno, baseName.c_str(), node.ino, 040000);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("removeDirectory", std::string(path), "Failed to unlink directory");
    }

    // Free inode (simplified - in production should use ext2fs_free_inode)
    ext2fs_close(fs);

    // Update cache
    invalidateCache(normalized);
}

void Ext4Volume::removeFileThrowsUnchecked(std::string_view path) {
    const std::string normalized = normalizeArg(path);

    // Check if file exists
    Inode node;
    if (!resolveNode(normalized, &node) || node.isDirectory) {
        throw IOException("removeFile", std::string(path), "File does not exist or is a directory");
    }

    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0,
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("removeFile", std::string(path), "ext2fs_open failed");
    }

    // Remove file from parent directory
    const std::string parent = normalized.substr(0, normalized.find_last_of('/'));
    uint32_t parentIno = resolveParentInode(normalized);
    std::string baseName = getBaseName(normalized);

    rc = ext2fs_unlink(fs, parentIno, baseName.c_str(), node.ino, 0100000);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("removeFile", std::string(path), "Failed to unlink file");
    }

    ext2fs_close(fs);

    // Update cache
    invalidateCache(normalized);
}

void Ext4Volume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    const std::string srcNormalized = normalizeArg(src);
    const std::string destNormalized = normalizeArg(dest);

    // Check source file
    Inode srcNode;
    if (!resolveNode(srcNormalized, &srcNode) || srcNode.isDirectory) {
        throw IOException("copyFile", std::string(src), "Source file does not exist or is a directory");
    }

    // Check if destination already exists
    Inode destNode;
    if (resolveNode(destNormalized, &destNode)) {
        throw IOException("copyFile", std::string(dest), "Destination already exists");
    }

    // Read source file data
    std::vector<uint8_t> data = readFile(src);

    // Write to destination
    writeFileUnchecked(destNormalized, data);
}

void Ext4Volume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    const std::string srcNormalized = normalizeArg(src);
    const std::string destNormalized = normalizeArg(dest);

    // Check source file
    Inode srcNode;
    if (!resolveNode(srcNormalized, &srcNode) || srcNode.isDirectory) {
        throw IOException("moveFile", std::string(src), "Source file does not exist or is a directory");
    }

    // Check if destination already exists
    Inode destNode;
    if (resolveNode(destNormalized, &destNode)) {
        throw IOException("moveFile", std::string(dest), "Destination already exists");
    }

    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0,
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("moveFile", std::string(src), "ext2fs_open failed");
    }

    // Read source data
    std::vector<uint8_t> data = readFile(src);

    // Write to destination
    writeFileUnchecked(destNormalized, data);

    // Remove source
    const std::string srcParent = srcNormalized.substr(0, srcNormalized.find_last_of('/'));
    uint32_t srcParentIno = resolveParentInode(srcNormalized);
    std::string srcBaseName = getBaseName(srcNormalized);

    rc = ext2fs_unlink(fs, srcParentIno, srcBaseName.c_str(), srcNode.ino, 0100000);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("moveFile", std::string(src), "Failed to unlink source file");
    }

    ext2fs_close(fs);

    // Update cache
    invalidateCache(srcNormalized);
    invalidateCache(destNormalized);
}

void Ext4Volume::renameFileThrowsUnchecked(std::string_view oldPath, std::string_view newPath) {
    const std::string oldNormalized = normalizeArg(oldPath);
    const std::string newNormalized = normalizeArg(newPath);

    // Check source
    Inode oldNode;
    if (!resolveNode(oldNormalized, &oldNode)) {
        throw IOException("renameFile", std::string(oldPath), "Source does not exist");
    }

    // Check if new path already exists
    Inode newNode;
    if (resolveNode(newNormalized, &newNode)) {
        throw IOException("renameFile", std::string(newPath), "Destination already exists");
    }

    // Check if parent directory exists for new path
    const std::string newParent = newNormalized.substr(0, newNormalized.find_last_of('/'));
    Inode parentNode;
    if (!resolveNode(newParent, &parentNode) || !parentNode.isDirectory) {
        throw IOException("renameFile", std::string(newPath), "Parent directory does not exist");
    }

    // Open filesystem with write flag
    ext2_filsys fs = nullptr;
    errcode_t rc = ext2fs_open(m_imagePath.c_str(), EXT2_FLAG_64BITS | EXT2_FLAG_RW, 0, 0,
                               unix_io_manager, &fs);
    if (rc) {
        throw IOException("renameFile", std::string(oldPath), "ext2fs_open failed");
    }

    // Remove old directory entry
    const std::string oldParent = oldNormalized.substr(0, oldNormalized.find_last_of('/'));
    uint32_t oldParentIno = resolveParentInode(oldNormalized);
    std::string oldBaseName = getBaseName(oldNormalized);

    rc = ext2fs_unlink(fs, oldParentIno, oldBaseName.c_str(), oldNode.ino, 0);
    if (rc) {
        ext2fs_close(fs);
        throw IOException("renameFile", std::string(oldPath), "Failed to unlink old entry");
    }

    // Create new directory entry with same inode
    std::string newBaseName = getBaseName(newNormalized);
    rc = ext2fs_link(fs, parentNode.ino, newBaseName.c_str(), oldNode.ino, oldNode.mode);
    if (rc == EXT2_ET_DIR_NO_SPACE) {
        rc = ext2fs_expand_dir(fs, parentNode.ino);
        if (rc) {
            ext2fs_close(fs);
            throw IOException("renameFile", std::string(newPath), "Failed to expand directory");
        }
        rc = ext2fs_link(fs, parentNode.ino, newBaseName.c_str(), oldNode.ino, oldNode.mode);
    }
    if (rc) {
        ext2fs_close(fs);
        throw IOException("renameFile", std::string(newPath), "Failed to create new entry");
    }

    ext2fs_close(fs);

    // Update cache
    invalidateCache(oldNormalized);
    invalidateCache(newNormalized);
}
