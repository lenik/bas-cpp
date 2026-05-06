#include "Fat32FileOutputStream.hpp"

#include "Fat32Volume.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

Fat32FileOutputStream::Fat32FileOutputStream(Fat32Volume* volume, std::string path, bool append,
                                             uint32_t dirCluster, uint32_t dirIndex)
    : m_volume(volume)
    , m_path(std::move(path))
    , m_append(append)
    , m_dirCluster(dirCluster)
    , m_dirIndex(dirIndex)
    , m_buffer(65536)  // 64KB buffer
    , m_bufferPos(0)
    , m_closed(false)
    , m_open(true)
{
    if (!m_volume || m_path.empty()) {
        throw IOException("Fat32FileOutputStream", "", "Empty path");
    }
    if (m_append && m_volume->exists(m_path)) {
        m_appendOnlyMode = true;
        Fat32Volume::Dirent currentDirent;
        if (!m_volume->findDirEntryLocation(m_path, m_dirCluster, m_dirIndex, &currentDirent)) {
            throw IOException("Fat32FileOutputStream", m_path, "Failed to resolve append target");
        }
        m_dirent = currentDirent;
        m_writePos = m_dirent.size;
        if (m_dirent.firstCluster >= 2 && m_dirent.size > 0) {
            m_clusters = m_volume->readClusterChain(m_dirent.firstCluster);
            if (!m_clusters.empty() && m_volume->m_clusterManager) {
                m_volume->m_clusterManager->setAllocHint(m_clusters.back() + 1);
            }
        }
    }
}

void Fat32FileOutputStream::loadExistingFileData() {
    Fat32Volume::Dirent currentDirent;
    if (!m_volume->findDirEntryLocation(m_path, m_dirCluster, m_dirIndex, &currentDirent)) {
        return;
    }
    m_dirent = currentDirent;
    if (m_dirent.size == 0 || m_dirent.firstCluster < 2) {
        return;
    }
    m_clusters = m_volume->readClusterChain(m_dirent.firstCluster);
    m_fileData.resize(m_dirent.size);
    size_t loaded = 0;
    for (uint32_t cluster : m_clusters) {
        if (loaded >= m_dirent.size) {
            break;
        }
        const size_t n = std::min<size_t>(m_volume->m_clusterSize, m_dirent.size - loaded);
        const uint64_t off = m_volume->clusterToOffset(cluster);
        if (!m_volume->m_device->read(off, m_fileData.data() + loaded, n)) {
            throw IOException("Fat32FileOutputStream", m_path, "Failed reading existing file content");
        }
        loaded += n;
    }
    m_writePos = loaded;
}

void Fat32FileOutputStream::ensureClustersForSize(uint64_t size, bool reserveHeadroom) {
    uint32_t needClusters =
        (size == 0) ? 0u : static_cast<uint32_t>((size + m_volume->m_clusterSize - 1) / m_volume->m_clusterSize);
    if (reserveHeadroom && needClusters > 0) {
        // Pre-allocation window reduces FAT churn for long sequential appends.
        const uint32_t extra = std::min<uint32_t>(32u, std::max<uint32_t>(4u, needClusters / 4u));
        needClusters += extra;
    }
    if (m_clusters.size() >= needClusters) {
        return;
    }
    m_clusters = m_volume->reallocClusters(m_clusters, static_cast<uint64_t>(needClusters) * m_volume->m_clusterSize);
    if (m_clusters.size() < needClusters) {
        throw IOException("Fat32FileOutputStream", m_path, "No space left on device");
    }
}

void Fat32FileOutputStream::flushAppendOnly() {
    if (m_bufferPos == 0) {
        return;
    }
    const size_t oldPos = m_writePos;
    const size_t newSize = oldPos + m_bufferPos;
    ensureClustersForSize(newSize, true);

    size_t srcOff = 0;
    size_t pos = oldPos;
    while (srcOff < m_bufferPos) {
        const uint32_t clusterIndex = static_cast<uint32_t>(pos / m_volume->m_clusterSize);
        const uint32_t inCluster = static_cast<uint32_t>(pos % m_volume->m_clusterSize);
        const size_t canWrite =
            std::min<size_t>(m_bufferPos - srcOff, static_cast<size_t>(m_volume->m_clusterSize - inCluster));
        const uint64_t devOff = m_volume->clusterToOffset(m_clusters[clusterIndex]);

        if (inCluster == 0 && canWrite == m_volume->m_clusterSize) {
            if (!m_volume->m_device->write(devOff, m_buffer.data() + srcOff, canWrite)) {
                throw IOException("Fat32FileOutputStream", m_path, "Failed writing append cluster");
            }
        } else {
            std::vector<uint8_t> block(m_volume->m_clusterSize, 0);
            if (!m_volume->m_device->read(devOff, block.data(), block.size())) {
                throw IOException("Fat32FileOutputStream", m_path, "Failed reading append cluster");
            }
            memcpy(block.data() + inCluster, m_buffer.data() + srcOff, canWrite);
            if (!m_volume->m_device->write(devOff, block.data(), block.size())) {
                throw IOException("Fat32FileOutputStream", m_path, "Failed writing append cluster");
            }
        }
        pos += canWrite;
        srcOff += canWrite;
    }

    m_writePos = newSize;
    ensureDirEntry();
    m_dirent.isDirectory = false;
    m_dirent.firstCluster = m_clusters.empty() ? 0u : m_clusters.front();
    m_dirent.size = static_cast<uint32_t>(newSize);
    m_volume->updateDirEntry(m_dirCluster, m_dirIndex, m_dirent);
    m_volume->m_dirents[m_volume->resolvePath(m_path)] = m_dirent;
    if (m_volume->m_clusterManager) {
        m_volume->m_clusterManager->flushDirty();
    }
    m_bufferPos = 0;
}

void Fat32FileOutputStream::writeFileDataByClusterChain() {
    size_t i = 0;
    while (i < m_clusters.size()) {
        size_t runStart = i;
        while (i + 1 < m_clusters.size() && m_clusters[i + 1] == m_clusters[i] + 1) {
            ++i;
        }
        const size_t runEnd = i;
        const size_t runClusters = runEnd - runStart + 1;
        const size_t bytes = runClusters * m_volume->m_clusterSize;
        std::vector<uint8_t> block(bytes, 0);
        const size_t srcOff = runStart * m_volume->m_clusterSize;
        if (srcOff < m_fileData.size()) {
            const size_t srcLen = std::min(bytes, m_fileData.size() - srcOff);
            memcpy(block.data(), m_fileData.data() + srcOff, srcLen);
        }
        const uint64_t off = m_volume->clusterToOffset(m_clusters[runStart]);
        if (!m_volume->m_device->write(off, block.data(), block.size())) {
            throw IOException("Fat32FileOutputStream", m_path, "Failed writing file block run");
        }
        ++i;
    }
}

void Fat32FileOutputStream::ensureDirEntry() {
    if (m_dirIndex != Fat32Volume::INVALID_DIR_INDEX && m_dirCluster >= 2) {
        return;
    }
    m_volume->createFileEntry(m_path, m_clusters.empty() ? 0u : m_clusters.front(),
                              static_cast<uint32_t>(m_fileData.size()));
    if (!m_volume->findDirEntryLocation(m_path, m_dirCluster, m_dirIndex, &m_dirent)) {
        throw IOException("Fat32FileOutputStream", m_path, "Failed to locate directory entry");
    }
}

Fat32FileOutputStream::~Fat32FileOutputStream() {
    try {
        close();
    } catch (...) {
        // Ignore exceptions in destructor
    }
}

bool Fat32FileOutputStream::write(int byte) {
    if (m_closed || !m_open) {
        return false;
    }
    if (m_bufferPos >= m_buffer.size()) {
        flush();
    }
    if (m_bufferPos >= m_buffer.size()) {
        return false;  // Buffer still full after flush
    }
    m_buffer[m_bufferPos++] = static_cast<uint8_t>(byte);
    return true;
}

size_t Fat32FileOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !m_open || !buf || len == 0) {
        return 0;
    }
    
    size_t written = 0;
    while (written < len) {
        if (m_bufferPos >= m_buffer.size()) {
            flush();
            if (m_bufferPos >= m_buffer.size()) {
                break;  // Can't write more
            }
        }
        
        size_t toCopy = std::min(len - written, m_buffer.size() - m_bufferPos);
        memcpy(m_buffer.data() + m_bufferPos, buf + off + written, toCopy);
        m_bufferPos += toCopy;
        written += toCopy;
    }
    
    return written;
}

void Fat32FileOutputStream::flush() {
    if (!m_closed && m_open && m_bufferPos > 0) {
        if (m_appendOnlyMode) {
            flushAppendOnly();
            return;
        }
        const size_t needed = m_writePos + m_bufferPos;
        if (m_fileData.size() < needed) {
            m_fileData.resize(needed, 0);
        }
        memcpy(m_fileData.data() + m_writePos, m_buffer.data(), m_bufferPos);
        m_writePos += m_bufferPos;
        m_clusters = m_volume->reallocClusters(m_clusters, m_fileData.size());
        writeFileDataByClusterChain();
        ensureDirEntry();
        m_dirent.isDirectory = false;
        m_dirent.firstCluster = m_clusters.empty() ? 0u : m_clusters.front();
        m_dirent.size = static_cast<uint32_t>(m_fileData.size());
        m_volume->updateDirEntry(m_dirCluster, m_dirIndex, m_dirent);
        m_volume->m_dirents[m_volume->resolvePath(m_path)] = m_dirent;
        if (m_volume->m_clusterManager) {
            m_volume->m_clusterManager->flushDirty();
        }
        m_bufferPos = 0;
    }
}

void Fat32FileOutputStream::close() {
    if (!m_closed && m_open) {
        try {
            flush();
        } catch (const IOException&) {
            m_open = false;
            throw;
        }
        m_closed = true;
        m_open = false;
    }
}
