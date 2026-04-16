#include "ExfatFileOutputStream.hpp"
#include "../../io/IOException.hpp"

#include <algorithm>
#include <cstring>

ExfatFileOutputStream::ExfatFileOutputStream(ExfatVolume* volume, std::string path, bool append)
    : m_volume(volume), m_path(std::move(path)), m_append(append) {
    if (!m_volume || m_path.empty()) {
        throw IOException("ExfatFileOutputStream", "", "Empty path");
    }
    if (m_append && m_volume->exists(m_path)) {
        m_appendOnlyMode = true;
        loadExisting();
    }
}

ExfatFileOutputStream::~ExfatFileOutputStream() {
    try {
        close();
    } catch (...) {}
}

void ExfatFileOutputStream::loadExisting() {
    const std::string normalized = m_volume->normalizeArg(m_path);
    auto it = m_volume->m_dirents.find(normalized);
    if (it == m_volume->m_dirents.end() || it->second.isDirectory) {
        throw IOException("ExfatFileOutputStream", m_path, "Invalid append target");
    }
    m_dirent = it->second;
    m_writePos = static_cast<size_t>(m_dirent.size);
    if (m_dirent.firstCluster >= 2 && m_dirent.size > 0) {
        m_clusters = m_volume->readClusterChain(m_dirent.firstCluster);
    }
}

bool ExfatFileOutputStream::write(int byte) {
    uint8_t b = static_cast<uint8_t>(byte);
    return write(&b, 0, 1) == 1;
}

size_t ExfatFileOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !m_open || !buf || len == 0) return 0;
    size_t written = 0;
    while (written < len) {
        if (m_bufferPos >= m_buffer.size()) {
            flush();
            if (m_bufferPos >= m_buffer.size()) break;
        }
        size_t toCopy = std::min(len - written, m_buffer.size() - m_bufferPos);
        memcpy(m_buffer.data() + m_bufferPos, buf + off + written, toCopy);
        m_bufferPos += toCopy;
        written += toCopy;
    }
    return written;
}

void ExfatFileOutputStream::flushAppendOnly() {
    if (m_bufferPos == 0) return;
    size_t srcOff = 0;
    while (srcOff < m_bufferPos) {
        const uint32_t clusterIndex = static_cast<uint32_t>(m_writePos / m_volume->m_clusterSize);
        const uint32_t inCluster = static_cast<uint32_t>(m_writePos % m_volume->m_clusterSize);
        while (m_clusters.size() <= clusterIndex) {
            uint32_t prev = m_clusters.empty() ? 0 : m_clusters.back();
            uint32_t c = m_volume->allocateCluster(prev);
            if (c == 0) throw IOException("ExfatFileOutputStream", m_path, "No space left on device");
            m_clusters.push_back(c);
        }
        const size_t canWrite =
            std::min<size_t>(m_bufferPos - srcOff, static_cast<size_t>(m_volume->m_clusterSize - inCluster));
        const uint64_t devOff = m_volume->clusterToOffset(m_clusters[clusterIndex]);
        if (inCluster == 0 && canWrite == m_volume->m_clusterSize) {
            if (!m_volume->m_device->write(devOff, m_buffer.data() + srcOff, canWrite)) {
                throw IOException("ExfatFileOutputStream", m_path, "Failed writing cluster");
            }
        } else {
            std::vector<uint8_t> block(m_volume->m_clusterSize, 0);
            if (!m_volume->m_device->read(devOff, block.data(), block.size())) {
                throw IOException("ExfatFileOutputStream", m_path, "Failed reading cluster");
            }
            memcpy(block.data() + inCluster, m_buffer.data() + srcOff, canWrite);
            if (!m_volume->m_device->write(devOff, block.data(), block.size())) {
                throw IOException("ExfatFileOutputStream", m_path, "Failed writing cluster");
            }
        }
        m_writePos += canWrite;
        srcOff += canWrite;
    }
    m_bufferPos = 0;
    m_dirent.firstCluster = m_clusters.empty() ? 0 : m_clusters.front();
    m_dirent.size = m_writePos;
    m_volume->updateFileEntry(m_path, m_dirent.firstCluster, m_dirent.size);
}

void ExfatFileOutputStream::flushRewriteAll() {
    if (m_bufferPos == 0) return;
    const size_t needed = m_writePos + m_bufferPos;
    if (m_fileData.size() < needed) m_fileData.resize(needed, 0);
    memcpy(m_fileData.data() + m_writePos, m_buffer.data(), m_bufferPos);
    m_writePos += m_bufferPos;
    m_volume->writeFileUnchecked(m_path, m_fileData);
    m_bufferPos = 0;
}

void ExfatFileOutputStream::flush() {
    if (m_closed || !m_open || m_bufferPos == 0) return;
    if (m_appendOnlyMode) flushAppendOnly();
    else flushRewriteAll();
}

void ExfatFileOutputStream::close() {
    if (m_closed) return;
    flush();
    m_closed = true;
    m_open = false;
}
