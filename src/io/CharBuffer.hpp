#ifndef CHARBUFFER_H
#define CHARBUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Mutable buffer of UTF-16 code units (char16_t) with position, limit, and capacity (Java NIO-style).
 * Used with ICU UChar; char16_t is the same type on typical platforms.
 */
class CharBuffer {
public:
    static CharBuffer allocate(size_t capacity);

    size_t capacity() const { return m_data.size(); }
    size_t position() const { return m_position; }
    size_t limit() const { return m_limit; }

    CharBuffer& position(size_t p);
    CharBuffer& limit(size_t l);

    CharBuffer& clear();
    CharBuffer& flip();
    CharBuffer& rewind();
    CharBuffer& compact();

    size_t remaining() const { return m_limit - m_position; }
    bool hasRemaining() const { return m_position < m_limit; }

    char16_t get();
    CharBuffer& get(char16_t* dst, size_t len);
    CharBuffer& put(char16_t c);
    CharBuffer& put(const char16_t* src, size_t len);

    char16_t* data() { return m_data.data(); }
    const char16_t* data() const { return m_data.data(); }
    char16_t* ptr() { return m_data.data() + m_position; }
    const char16_t* ptr() const { return m_data.data() + m_position; }

private:
    explicit CharBuffer(size_t capacity);

    std::vector<char16_t> m_data;
    size_t m_position;
    size_t m_limit;
};

#endif // CHARBUFFER_H
