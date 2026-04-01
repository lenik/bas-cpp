#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

/**
 * Abstract block device interface.
 * Provides raw block-level read/write access to storage.
 */
class BlockDevice {
public:
    virtual ~BlockDevice() = default;
    
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
     * Flush pending writes to persistent storage.
     * @return true if successful
     */
    virtual bool flush() = 0;
    
    /**
     * Get device name/identifier.
     * @return Device name
     */
    virtual std::string name() const = 0;
    
    /**
     * Check if device is open/ready.
     * @return true if device is ready
     */
    virtual bool isOpen() const { return true; }
};

/**
 * File-backed block device.
 * Uses a regular file as storage.
 */
class FileDevice : public BlockDevice {
public:
    explicit FileDevice(const std::string& path);
    ~FileDevice() override;
    
    bool read(uint64_t offset, uint8_t* dst, size_t len) override;
    bool write(uint64_t offset, const uint8_t* src, size_t len) override;
    uint64_t size() const override;
    bool flush() override;
    std::string name() const override { return m_path; }
    
    bool isOpen() const { return m_fd >= 0; }

private:
    std::string m_path;
    int m_fd = -1;
    uint64_t m_size = 0;
};

/**
 * Memory-backed block device.
 * Uses a memory region as storage.
 */
class MemDevice : public BlockDevice {
public:
    explicit MemDevice(size_t size);
    explicit MemDevice(const uint8_t* data, size_t size);
    ~MemDevice() override;
    
    bool read(uint64_t offset, uint8_t* dst, size_t len) override;
    bool write(uint64_t offset, const uint8_t* src, size_t len) override;
    uint64_t size() const override { return m_size; }
    bool flush() override { return true; }  // Memory is already persistent
    std::string name() const override { return "memory:" + std::to_string(m_size); }
    
    uint8_t* data() { return m_data; }
    const uint8_t* data() const { return m_data; }

private:
    uint8_t* m_data = nullptr;
    size_t m_size = 0;
    bool m_owned = false;  // Whether we own the memory
};

// Factory functions
std::shared_ptr<BlockDevice> createFileDevice(const std::string& path);
std::shared_ptr<BlockDevice> createMemDevice(size_t size);
std::shared_ptr<BlockDevice> createMemDevice(const uint8_t* data, size_t size);

#endif // BLOCKDEVICE_H
