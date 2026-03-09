#include "LocalOutputStream.hpp"

LocalOutputStream::LocalOutputStream(const std::string& path, bool append)
{
    std::ios::openmode mode = std::ios::binary | std::ios::out;
    mode |= append ? std::ios::app : std::ios::trunc;
    m_file.open(path, mode);
}

LocalOutputStream::~LocalOutputStream() {
    close();
}

bool LocalOutputStream::write(int byte) {
    if (!m_file.is_open()) return false;
    char c = static_cast<char>(byte & 0xFF);
    m_file.put(c);
    return m_file.good();
}

size_t LocalOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (!m_file.is_open()) {
        return 0;
    }
    m_file.write(reinterpret_cast<const char*>(buf + off), len);
    return m_file.good() ? len : 0;
}

void LocalOutputStream::flush() {
    if (m_file.is_open()) {
        m_file.flush();
    }
}

void LocalOutputStream::close() {
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}
