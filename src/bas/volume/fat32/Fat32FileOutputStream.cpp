#include "Fat32FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "Fat32Volume.hpp"

#include <vector>
#include <cstring>

Fat32FileOutputStream::Fat32FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath))
    , m_path(std::move(path))
    , m_append(append)
    , m_buffer(65536)  // 64KB buffer
    , m_bufferPos(0)
    , m_closed(false)
    , m_open(true)
{
    if (m_path.empty()) {
        throw IOException("Fat32FileOutputStream", "", "Empty path");
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
        // Note: FAT32 doesn't support true incremental writes easily
        // The buffer is kept in memory and written on close via Volume::writeFile
        // This flush() is a no-op for now - actual write happens on close
        // A full implementation would require cluster chain management
    }
}

void Fat32FileOutputStream::close() {
    if (!m_closed && m_open) {
        if (m_bufferPos > 0) {
            // Write buffered data to volume
            try {
                // Get volume instance - this is a bit awkward but necessary
                // In practice, streams are usually created by Volume::newOutputStream()
                // which could pass a reference to itself
                Fat32Volume vol(m_imagePath);
                
                // Read existing file if appending
                std::vector<uint8_t> existingData;
                if (m_append && vol.exists(m_path)) {
                    existingData = vol.readFile(m_path);
                }
                
                // Combine existing data with new data
                std::vector<uint8_t> finalData = existingData;
                finalData.insert(finalData.end(), m_buffer.begin(), m_buffer.begin() + m_bufferPos);
                
                // Write to volume
                vol.writeFile(m_path, finalData);
            } catch (const IOException& e) {
                m_open = false;
                throw;
            }
        }
        m_closed = true;
        m_open = false;
    }
}
