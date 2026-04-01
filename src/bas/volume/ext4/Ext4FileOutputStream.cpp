#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"
#include "Ext4Volume.hpp"

#include <vector>
#include <cstring>

Ext4FileOutputStream::Ext4FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath))
    , m_path(std::move(path))
    , m_append(append)
    , m_buffer(65536)  // 64KB buffer
    , m_bufferPos(0)
    , m_closed(false)
    , m_open(true)
{
    if (m_path.empty()) {
        throw IOException("Ext4FileOutputStream", "", "Empty path");
    }
}

Ext4FileOutputStream::~Ext4FileOutputStream() {
    try {
        close();
    } catch (...) {
        // Ignore exceptions in destructor
    }
}

bool Ext4FileOutputStream::write(int byte) {
    if (m_closed || !m_open) {
        return false;
    }
    if (m_bufferPos >= m_buffer.size()) {
        flush();
        if (m_bufferPos >= m_buffer.size()) {
            return false;
        }
    }
    m_buffer[m_bufferPos++] = static_cast<uint8_t>(byte);
    return true;
}

size_t Ext4FileOutputStream::write(const uint8_t* buf, size_t off, size_t len) {
    if (m_closed || !m_open || !buf || len == 0) {
        return 0;
    }
    
    size_t written = 0;
    while (written < len) {
        if (m_bufferPos >= m_buffer.size()) {
            flush();
            if (m_bufferPos >= m_buffer.size()) {
                break;
            }
        }
        
        size_t toCopy = std::min(len - written, m_buffer.size() - m_bufferPos);
        memcpy(m_buffer.data() + m_bufferPos, buf + off + written, toCopy);
        m_bufferPos += toCopy;
        written += toCopy;
    }
    
    return written;
}

void Ext4FileOutputStream::flush() {
    // ext4 doesn't support true incremental writes easily
    // Buffer is written on close via Volume::writeFile
}

void Ext4FileOutputStream::close() {
    if (!m_closed && m_open) {
        if (m_bufferPos > 0) {
            try {
                Ext4Volume vol(m_imagePath);
                
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
