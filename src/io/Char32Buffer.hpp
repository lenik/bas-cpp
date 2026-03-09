#ifndef CHAR32BUFFER_H
#define CHAR32BUFFER_H

#include <cstddef>
#include <vector>

/**
 * Mutable buffer of UTF-32 code units (Unicode code points, char32_t) with position, limit, and capacity (Java NIO-style).
 */
class Char32Buffer {
public:
    static Char32Buffer allocate(size_t capacity);

    size_t capacity() const { return m_data.size(); }
    size_t position() const { return m_position; }
    size_t limit() const { return m_limit; }

    Char32Buffer& position(size_t p);
    Char32Buffer& limit(size_t l);

    Char32Buffer& clear();
    Char32Buffer& flip();
    Char32Buffer& rewind();
    Char32Buffer& compact();

    size_t remaining() const { return m_limit - m_position; }
    bool hasRemaining() const { return m_position < m_limit; }

    char32_t get();
    Char32Buffer& get(char32_t* dst, size_t len);
    Char32Buffer& put(char32_t c);
    Char32Buffer& put(const char32_t* src, size_t len);

    char32_t* data() { return m_data.data(); }
    const char32_t* data() const { return m_data.data(); }
    char32_t* ptr() { return m_data.data() + m_position; }
    const char32_t* ptr() const { return m_data.data() + m_position; }

private:
    explicit Char32Buffer(size_t capacity);

    std::vector<char32_t> m_data;
    size_t m_position;
    size_t m_limit;
};

#endif // CHAR32BUFFER_H
