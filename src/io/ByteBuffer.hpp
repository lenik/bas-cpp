#ifndef BYTEBUFFER_H
#define BYTEBUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Mutable buffer of bytes with position, limit, and capacity (Java NIO-style).
 * After filling: flip() to prepare for reading. After reading: compact() or clear() for more writing.
 */
class ByteBuffer {
public:
    static ByteBuffer allocate(size_t capacity);

    size_t capacity() const { return m_data.size(); }
    size_t position() const { return m_position; }
    size_t limit() const { return m_limit; }

    ByteBuffer& position(size_t p);
    ByteBuffer& limit(size_t l);

    /** position = 0, limit = capacity. Prepare for writing. */
    ByteBuffer& clear();
    /** limit = position, position = 0. Prepare for reading after writing. */
    ByteBuffer& flip();
    /** position = 0. Reread from start. */
    ByteBuffer& rewind();
    /** Copy [position, limit) to [0, ...), position = remaining(), limit = capacity. */
    ByteBuffer& compact();

    size_t remaining() const { return m_limit - m_position; }
    bool hasRemaining() const { return m_position < m_limit; }

    uint8_t get();
    ByteBuffer& get(uint8_t* dst, size_t len);
    ByteBuffer& put(uint8_t b);
    ByteBuffer& put(const uint8_t* src, size_t len);

    /** Pointer to backing array (element 0). */
    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }
    /** Pointer to current position (for reading or writing). */
    uint8_t* ptr() { return m_data.data() + m_position; }
    const uint8_t* ptr() const { return m_data.data() + m_position; }

private:
    explicit ByteBuffer(size_t capacity);

    std::vector<uint8_t> m_data;
    size_t m_position;
    size_t m_limit;
};

#endif // BYTEBUFFER_H
