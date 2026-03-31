#include "LocalInputStream.hpp"

#include <vector>

LocalInputStream::LocalInputStream(const std::string& path)
    : m_file(path, std::ios::binary)
    //, m_closed(false)
{
}

LocalInputStream::~LocalInputStream() {
    close();
}

int LocalInputStream::read() {
    if (!m_file.is_open()) return -1;
    char c;
    if (m_file.get(c)) return static_cast<unsigned char>(c);
    return -1;
}

size_t LocalInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (!m_file.is_open() || !buf || len <= 0 || off < 0) {
        return 0;
    }
    m_file.read(reinterpret_cast<char*>(buf + off), len);
    auto n = m_file.gcount();
    return n <= 0 ? 0 : static_cast<int>(n);
}

int64_t LocalInputStream::skip(int64_t len) {
    if (!m_file.is_open() || len <= 0) return 0;
    m_file.clear();
    m_file.seekg(len, std::ios::cur);
    if (m_file.good()) {
        return len;
    }
    // fallback: read and discard
    m_file.clear();
    std::vector<char> tmp(static_cast<size_t>(len));
    m_file.read(tmp.data(), len);
    auto n = m_file.gcount();
    return n < 0 ? 0 : static_cast<int>(n);
}

int64_t LocalInputStream::position() const {
    if (!m_file.is_open()) {
        return -1;
    }
    auto pos = m_file.tellg();
    return pos < 0 ? -1 : static_cast<int64_t>(pos);
}

bool LocalInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    if (!m_file.is_open()) {
        return false;
    }
    m_file.clear();
    m_file.seekg(offset, dir);
    return m_file.good();
}

void LocalInputStream::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
}
