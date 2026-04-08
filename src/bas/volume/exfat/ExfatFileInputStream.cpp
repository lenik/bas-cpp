#include "ExfatFileInputStream.hpp"
#include <algorithm>

ExfatFileInputStream::ExfatFileInputStream(std::string imagePath, std::vector<uint32_t> clusterChain,
                                           uint64_t dataOffset, uint32_t clusterSize, uint64_t fileSize)
    : m_imagePath(std::move(imagePath))
    , m_chain(std::move(clusterChain))
    , m_dataOffset(dataOffset)
    , m_clusterSize(clusterSize)
    , m_fileSize(fileSize)
    , m_file(m_imagePath, std::ios::binary) {}

ExfatFileInputStream::~ExfatFileInputStream() {
    close();
}

bool ExfatFileInputStream::readAt(uint64_t pos, uint8_t* dst, size_t len) {
    if (!m_file.is_open() || !dst || len == 0 || pos >= m_fileSize) {
        return false;
    }
    size_t remain = len;
    size_t out = 0;
    while (remain > 0 && pos < m_fileSize) {
        const uint64_t clusterIndex = pos / m_clusterSize;
        if (clusterIndex >= m_chain.size()) break;
        
        const uint64_t inClusterOff = pos % m_clusterSize;
        const uint64_t canTake = std::min<uint64_t>(
            remain, std::min<uint64_t>(m_clusterSize - inClusterOff, m_fileSize - pos));
        const uint64_t imageOff = m_dataOffset + 
            (static_cast<uint64_t>(m_chain[clusterIndex]) - 2) * m_clusterSize + inClusterOff;
        
        m_file.clear();
        m_file.seekg(static_cast<std::streamoff>(imageOff), std::ios::beg);
        if (!m_file.good()) return false;
        
        m_file.read(reinterpret_cast<char*>(dst + out), static_cast<std::streamsize>(canTake));
        const std::streamsize got = m_file.gcount();
        if (got <= 0) return false;
        
        out += static_cast<size_t>(got);
        remain -= static_cast<size_t>(got);
        pos += static_cast<uint64_t>(got);
    }
    return out > 0;
}

int ExfatFileInputStream::read() {
    uint8_t b = 0;
    return (read(&b, 0, 1) == 1) ? b : -1;
}

size_t ExfatFileInputStream::read(uint8_t* buf, size_t off, size_t len) {
    if (!buf || len == 0 || m_pos >= m_fileSize) return 0;
    const size_t n = static_cast<size_t>(std::min<uint64_t>(len, m_fileSize - m_pos));
    if (!readAt(m_pos, buf + off, n)) return 0;
    m_pos += n;
    return n;
}

int64_t ExfatFileInputStream::skip(int64_t len) {
    if (len <= 0) return 0;
    const uint64_t old = m_pos;
    m_pos = std::min<uint64_t>(m_fileSize, m_pos + static_cast<uint64_t>(len));
    return static_cast<int64_t>(m_pos - old);
}

int64_t ExfatFileInputStream::position() const {
    return static_cast<int64_t>(m_pos);
}

bool ExfatFileInputStream::seek(int64_t offset, std::ios::seekdir dir) {
    int64_t target = 0;
    if (dir == std::ios::beg) target = offset;
    else if (dir == std::ios::cur) target = static_cast<int64_t>(m_pos) + offset;
    else if (dir == std::ios::end) target = static_cast<int64_t>(m_fileSize) + offset;
    else return false;
    
    if (target < 0) return false;
    m_pos = std::min<uint64_t>(m_fileSize, static_cast<uint64_t>(target));
    return true;
}

void ExfatFileInputStream::close() {
    if (m_file.is_open()) m_file.close();
}
