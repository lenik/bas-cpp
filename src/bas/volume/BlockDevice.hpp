#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

/**
 * Block device types.
 */
enum class BlockDeviceType {
    FILE, // File-backed
    MEM,  // Memory-backed
    MMAP, // Memory-mapped file
    LOOP  // Loopback (wraps another BlockDevice)
};

/**
 * Convert device type to string.
 */
inline std::string blockDeviceTypeToString(BlockDeviceType type) {
    switch (type) {
    case BlockDeviceType::FILE:
        return "file";
    case BlockDeviceType::MEM:
        return "mem";
    case BlockDeviceType::MMAP:
        return "mmap";
    case BlockDeviceType::LOOP:
        return "loop";
    default:
        return "unknown";
    }
}

/**
 * Abstract block device interface.
 * Provides raw block-level read/write access to storage.
 */
class BlockDevice {
  public:
    virtual ~BlockDevice() = default;

    /**
     * Get device type.
     * @return Device type
     */
    virtual BlockDeviceType type() const = 0;

    /**
     * Get device type as string.
     * @return Type string
     */
    std::string typeString() const { return blockDeviceTypeToString(type()); }

    /**
     * Get unique device ID.
     * @return Device ID
     */
    uint64_t id() const { return m_id; }

    /**
     * Get device name/identifier.
     * @return Device name
     */
    virtual std::string name() const = 0;

    /**
     * Get device URI.
     * @return Device URI
     */
    virtual std::string uri() const = 0;

    /**
     * Read blocks from device.
     * @param offset Byte offset to read from
     * @param dst Destination buffer
     * @param len Number of bytes to read
     * @return true if successful
     */
    virtual bool read(uint64_t offset, uint8_t* dst, size_t len) = 0;

    /**
     * Write blocks to device.
     * @param offset Byte offset to write to
     * @param src Source buffer
     * @param len Number of bytes to write
     * @return true if successful
     */
    virtual bool write(uint64_t offset, const uint8_t* src, size_t len) = 0;

    /**
     * Get device size in bytes.
     * @return Device size
     */
    virtual uint64_t size() const = 0;

    /**
     * Flush pending writes (deprecated, use sync).
     */
    virtual bool flush() { return true; }

    /**
     * Check if device is open/ready.
     * @return true if device is ready
     */
    virtual bool isOpen() const { return true; }

    /**
     * Create file-backed block device with on-demand access.
     * @param path File path
     * @param offset Offset in file (default 0)
     * @param length Length to access (default: rest of file)
     * @return Block device
     */
    static std::shared_ptr<BlockDevice> file(const std::string& path, uint64_t offset = 0,
                                             uint64_t length = 0, bool read_only = false,
                                             bool cached = false);

    /**
     * Create file-backed block device with preloaded content.
     * @param path File path
     * @param offset Offset in file (default 0)
     * @param length Length to load (default: rest of file)
     * @return Block device
     */
    static std::shared_ptr<BlockDevice> load(const std::string& path, uint64_t offset = 0,
                                             uint64_t length = 0, bool read_only = false);

    /**
     * Create memory-mapped file block device.
     * @param path File path
     * @param offset Offset in file (default 0)
     * @param length Length to map (default: rest of file)
     * @return Block device
     */
    static std::shared_ptr<BlockDevice> mmap(const std::string& path, uint64_t offset = 0,
                                             uint64_t length = 0);

    /**
     * Create memory-backed block device.
     * @param data Memory pointer (copied)
     * @param size Size in bytes
     * @return Block device
     */
    static std::shared_ptr<BlockDevice> mem(const void* data, size_t size);

    /**
     * Create loopback block device (wraps another device).
     * @param device Underlying device
     * @param offset Offset in device
     * @param length Length to expose
     * @return Block device
     */
    static std::shared_ptr<BlockDevice> loop(std::shared_ptr<BlockDevice> device,
                                             uint64_t offset = 0, uint64_t length = 0);

  protected:
    BlockDevice() : m_id(s_next_id++) {}

  private:
    uint64_t m_id;
    static std::atomic<uint64_t> s_next_id;
};

#endif // BLOCKDEVICE_H
