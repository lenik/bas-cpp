#include "Ext4IoManager.hpp"

#include "../dev/FileDevice.hpp"
#include "../dev/MemDevice.hpp"

#include <cinttypes>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <ext2fs/ext2_io.h>
#include <ext2fs/ext2fs.h>

namespace ext4io {

namespace {

struct MemIoCtx {
    uint8_t* data = nullptr;
    size_t size = 0;
    bool ro = false;
};

struct BlockDeviceIoCtx {
    BlockDevice* dev = nullptr; // raw pointer; lifetime held in global map
    uintptr_t key = 0;
    bool ro = false;
};

static errcode_t mem_set_blksize(io_channel channel, int blksize) {
    if (!channel) return 1;
    channel->block_size = blksize;
    return 0;
}

static errcode_t mem_close(io_channel channel) {
    if (!channel) return 0;
    auto* ctx = static_cast<MemIoCtx*>(channel->private_data);
    if (ctx) {
        delete ctx;
        channel->private_data = nullptr;
    }
    if (channel->name) {
        free(channel->name);
        channel->name = nullptr;
    }
    free(channel);
    return 0;
}

static errcode_t mem_read_blk(io_channel channel, unsigned long block, int count, void* data) {
    auto* ctx = static_cast<MemIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !data || !channel) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const unsigned long long byteOff = static_cast<unsigned long long>(block) * bs;
    const unsigned long long byteLen = static_cast<unsigned long long>(count) * bs;
    if (byteOff + byteLen > ctx->size) return 1;
    std::memcpy(data, ctx->data + byteOff, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_write_blk(io_channel channel, unsigned long block, int count, const void* data) {
    auto* ctx = static_cast<MemIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !data || !channel) return 1;
    if (ctx->ro) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const unsigned long long byteOff = static_cast<unsigned long long>(block) * bs;
    const unsigned long long byteLen = static_cast<unsigned long long>(count) * bs;
    if (byteOff + byteLen > ctx->size) return 1;
    std::memcpy(ctx->data + byteOff, data, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_write_byte(io_channel channel, unsigned long offset, int count, const void* data) {
    auto* ctx = static_cast<MemIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !data || !channel) return 1;
    if (ctx->ro) return 1;
    const unsigned long long byteOff = static_cast<unsigned long long>(offset);
    const unsigned long long byteLen = static_cast<unsigned long long>(count);
    if (byteOff + byteLen > ctx->size) return 1;
    std::memcpy(ctx->data + byteOff, data, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_flush(io_channel /*channel*/) { return 0; }

static errcode_t mem_read_blk64(io_channel channel, unsigned long long block, int count, void* data) {
    if (!channel) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const unsigned long long byteOff = block * bs;
    const unsigned long long byteLen = static_cast<unsigned long long>(count) * bs;
    auto* ctx = static_cast<MemIoCtx*>(channel->private_data);
    if (!ctx || !data) return 1;
    if (byteOff + byteLen > ctx->size) return 1;
    std::memcpy(data, ctx->data + byteOff, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_write_blk64(io_channel channel, unsigned long long block, int count, const void* data) {
    if (!channel) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const unsigned long long byteOff = block * bs;
    const unsigned long long byteLen = static_cast<unsigned long long>(count) * bs;
    auto* ctx = static_cast<MemIoCtx*>(channel->private_data);
    if (!ctx || !data) return 1;
    if (ctx->ro) return 1;
    if (byteOff + byteLen > ctx->size) return 1;
    std::memcpy(ctx->data + byteOff, data, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_set_option(io_channel /*channel*/, const char* /*option*/, const char* /*arg*/) { return 0; }

static errcode_t mem_get_stats(io_channel /*channel*/, io_stats* stats) {
    if (!stats) return 1;
    if (!*stats) return 1;
    (*stats)->num_fields = 0;
    (*stats)->reserved = 0;
    (*stats)->bytes_read = 0;
    (*stats)->bytes_written = 0;
    return 0;
}

static errcode_t mem_discard(io_channel /*channel*/, unsigned long long /*block*/, unsigned long long /*count*/) {
    return 0;
}

static errcode_t mem_cache_readahead(io_channel /*channel*/, unsigned long long /*block*/, unsigned long long /*count*/) {
    return 0;
}

static errcode_t mem_zeroout(io_channel channel, unsigned long long block, unsigned long long count) {
    auto* ctx = static_cast<MemIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !channel) return 1;
    if (ctx->ro) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const unsigned long long byteOff = block * bs;
    const unsigned long long byteLen = count * bs;
    if (byteOff + byteLen > ctx->size) return 1;
    std::memset(ctx->data + byteOff, 0, static_cast<size_t>(byteLen));
    return 0;
}

static errcode_t mem_open(const char* name, int flags, io_channel* ret_channel) {
    if (!name || !ret_channel) return 1;

    // MemDevice::uri() is expected: mem:<ptr>:<size>
    uintptr_t ptr = 0;
    size_t size = 0;
    if (std::sscanf(name, "mem:%" SCNuPTR ":%zu", &ptr, &size) != 2) {
        return 1;
    }
    if (ptr == 0 || size == 0) return 1;

    auto* ctx = new MemIoCtx();
    ctx->data = reinterpret_cast<uint8_t*>(ptr);
    ctx->size = size;
    // EXT2_FLAG_RW uses the same low bit as io's RW intent.
    ctx->ro = ((flags & EXT2_FLAG_RW) == 0);

    auto* ch = static_cast<io_channel>(calloc(1, sizeof(struct struct_io_channel)));
    if (!ch) {
        delete ctx;
        return 1;
    }

    ch->magic = 0x4d454d49; // 'MEMI'
    ch->manager = nullptr; // filled by ext2fs after assignment
    ch->name = strdup(name);
    ch->block_size = 4096;
    ch->read_error = nullptr;
    ch->write_error = nullptr;
    ch->refcount = 1;
    ch->flags = ctx->ro ? 0u : IO_FLAG_RW;
    ch->private_data = ctx;
    ch->app_data = nullptr;
    ch->align = 0;

    *ret_channel = ch;
    return 0;
}

static const struct struct_io_manager memory_io_manager = {
    .magic = 0x4d454d49, // 'MEMI'
    .name = "ext4-mem-io",
    .open = mem_open,
    .close = mem_close,
    .set_blksize = mem_set_blksize,
    .read_blk = mem_read_blk,
    .write_blk = mem_write_blk,
    .flush = mem_flush,
    .write_byte = mem_write_byte,
    .set_option = mem_set_option,
    .get_stats = mem_get_stats,
    .read_blk64 = mem_read_blk64,
    .write_blk64 = mem_write_blk64,
    .discard = mem_discard,
    .cache_readahead = mem_cache_readahead,
    .zeroout = mem_zeroout,
};

// ---- BlockDevice-backed manager (fallback) ----

static std::mutex g_map_mu;
static std::unordered_map<uintptr_t, std::shared_ptr<BlockDevice>> g_map;

static errcode_t bd_set_blksize(io_channel channel, int blksize) {
    if (!channel) return 1;
    channel->block_size = blksize;
    return 0;
}

static errcode_t bd_close(io_channel channel) {
    if (!channel) return 0;
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel->private_data);
    if (ctx) {
        if (ctx->key) {
            std::lock_guard<std::mutex> lk(g_map_mu);
            g_map.erase(ctx->key);
        }
        delete ctx;
        channel->private_data = nullptr;
    }
    if (channel->name) {
        free(channel->name);
        channel->name = nullptr;
    }
    free(channel);
    return 0;
}

static errcode_t bd_read_blk(io_channel channel, unsigned long block, int count, void* data) {
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !ctx->dev || !data || !channel) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const uint64_t byteOff = static_cast<uint64_t>(block) * bs;
    const size_t byteLen = static_cast<size_t>(count) * bs;
    return ctx->dev->read(byteOff, static_cast<uint8_t*>(data), byteLen) ? 0 : 1;
}

static errcode_t bd_write_blk(io_channel channel, unsigned long block, int count, const void* data) {
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !ctx->dev || !data || !channel) return 1;
    if (ctx->ro) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const uint64_t byteOff = static_cast<uint64_t>(block) * bs;
    const size_t byteLen = static_cast<size_t>(count) * bs;
    return ctx->dev->write(byteOff, static_cast<const uint8_t*>(data), byteLen) ? 0 : 1;
}

static errcode_t bd_write_byte(io_channel channel, unsigned long offset, int count, const void* data) {
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !ctx->dev || !data || !channel) return 1;
    if (ctx->ro) return 1;
    const uint64_t byteOff = static_cast<uint64_t>(offset);
    const size_t byteLen = static_cast<size_t>(count);
    return ctx->dev->write(byteOff, static_cast<const uint8_t*>(data), byteLen) ? 0 : 1;
}

static errcode_t bd_flush(io_channel channel) {
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !ctx->dev) return 0;
    return ctx->dev->flush() ? 0 : 1;
}

static errcode_t bd_read_blk64(io_channel channel, unsigned long long block, int count, void* data) {
    return bd_read_blk(channel, static_cast<unsigned long>(block), count, data);
}

static errcode_t bd_write_blk64(io_channel channel, unsigned long long block, int count, const void* data) {
    return bd_write_blk(channel, static_cast<unsigned long>(block), count, data);
}

static errcode_t bd_set_option(io_channel /*channel*/, const char* /*option*/, const char* /*arg*/) { return 0; }

static errcode_t bd_get_stats(io_channel /*channel*/, io_stats* stats) {
    if (!stats) return 1;
    if (!*stats) return 1;
    (*stats)->num_fields = 0;
    (*stats)->reserved = 0;
    (*stats)->bytes_read = 0;
    (*stats)->bytes_written = 0;
    return 0;
}

static errcode_t bd_discard(io_channel /*channel*/, unsigned long long /*block*/, unsigned long long /*count*/) { return 0; }

static errcode_t bd_cache_readahead(io_channel /*channel*/, unsigned long long /*block*/, unsigned long long /*count*/) { return 0; }

static errcode_t bd_zeroout(io_channel channel, unsigned long long block, unsigned long long count) {
    // Best-effort: write zeros.
    auto* ctx = static_cast<BlockDeviceIoCtx*>(channel ? channel->private_data : nullptr);
    if (!ctx || !ctx->dev || ctx->ro) return 1;
    const size_t bs = static_cast<size_t>(channel->block_size ? channel->block_size : 4096);
    const uint64_t byteOff = block * bs;
    const size_t byteLen = static_cast<size_t>(count) * bs;
    std::vector<uint8_t> zeros(byteLen, 0);
    return ctx->dev->write(byteOff, zeros.data(), zeros.size()) ? 0 : 1;
}

static errcode_t bd_open(const char* name, int flags, io_channel* ret_channel) {
    if (!name || !ret_channel) return 1;
    uintptr_t key = 0;
    if (std::sscanf(name, "blockdev:%" SCNuPTR, &key) != 1) return 1;
    if (key == 0) return 1;

    std::shared_ptr<BlockDevice> dev;
    {
        std::lock_guard<std::mutex> lk(g_map_mu);
        auto it = g_map.find(key);
        if (it == g_map.end()) return 1;
        dev = it->second;
    }
    if (!dev) return 1;

    auto* ctx = new BlockDeviceIoCtx();
    ctx->dev = dev.get();
    ctx->key = key;
    ctx->ro = ((flags & EXT2_FLAG_RW) == 0);

    auto* ch = static_cast<io_channel>(calloc(1, sizeof(struct struct_io_channel)));
    if (!ch) {
        delete ctx;
        return 1;
    }
    ch->magic = 0x42444d44; // 'BDMD'
    ch->manager = nullptr;
    ch->name = strdup(name);
    ch->block_size = 4096;
    ch->read_error = nullptr;
    ch->write_error = nullptr;
    ch->refcount = 1;
    ch->flags = ctx->ro ? 0u : IO_FLAG_RW;
    ch->private_data = ctx;
    ch->app_data = nullptr;
    ch->align = 0;

    *ret_channel = ch;
    return 0;
}

static const struct struct_io_manager blockdevice_io_manager = {
    .magic = 0x42444d44, // 'BDMD'
    .name = "ext4-blockdevice-io",
    .open = bd_open,
    .close = bd_close,
    .set_blksize = bd_set_blksize,
    .read_blk = bd_read_blk,
    .write_blk = bd_write_blk,
    .flush = bd_flush,
    .write_byte = bd_write_byte,
    .set_option = bd_set_option,
    .get_stats = bd_get_stats,
    .read_blk64 = bd_read_blk64,
    .write_blk64 = bd_write_blk64,
    .discard = bd_discard,
    .cache_readahead = bd_cache_readahead,
    .zeroout = bd_zeroout,
};

} // namespace

int openFsFromBlockDevice(std::shared_ptr<BlockDevice> device, int flags,
                          ext2_filsys* ret_fs) {
    if (!device || !ret_fs) return 1;

    // FileDevice: use unix_io_manager and real image path.
    if (auto* f = dynamic_cast<FileDevice*>(device.get())) {
        const std::string path = f->name();
        return ext2fs_open(path.c_str(), flags, 0, 0, unix_io_manager, ret_fs);
    }

    // MemDevice: use a memory-backed io_manager.
    if (dynamic_cast<MemDevice*>(device.get())) {
        // MemDevice::uri() format: mem:<ptr>:<size>
        const std::string memUri = device->uri();
        return ext2fs_open(memUri.c_str(), flags, 0, 0, (io_manager)&memory_io_manager, ret_fs);
    }

    // Fallback for other BlockDevice types.
    const uintptr_t key = reinterpret_cast<uintptr_t>(device.get());
    if (key == 0) return 1;
    {
        std::lock_guard<std::mutex> lk(g_map_mu);
        g_map[key] = device;
    }
    const std::string name = std::string("blockdev:") + std::to_string(static_cast<uint64_t>(key));
    errcode_t rc = ext2fs_open(name.c_str(), flags, 0, 0, (io_manager)&blockdevice_io_manager, ret_fs);
    if (rc) {
        std::lock_guard<std::mutex> lk(g_map_mu);
        g_map.erase(key);
    }
    return rc;
}

} // namespace ext4io

