#ifndef EXT4IOMANAGER_HPP
#define EXT4IOMANAGER_HPP

#include "../BlockDevice.hpp"

#include <memory>

#include <ext2fs/ext2fs.h>
#ifdef clamp
#undef clamp
#endif

/**
 * Open an ext2/3/4 filesystem image using the correct ext2fs io_manager.
 *
 * Rules:
 * - FileDevice: use unix_io_manager and pass the real image file path.
 * - MemDevice: use a custom memory-backed io_manager and pass mem-uri.
 * - Other BlockDevice: use a custom BlockDevice-backed io_manager.
 */
namespace ext4io {

int openFsFromBlockDevice(std::shared_ptr<BlockDevice> device, int flags,
                          ext2_filsys* ret_fs);

} // namespace ext4io

#endif // EXT4IOMANAGER_HPP

