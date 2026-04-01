#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"

Ext4FileOutputStream::Ext4FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath)), m_path(std::move(path)), m_append(append) {
    // Note: This is a placeholder implementation
    // The actual write operations are handled by Ext4Volume::writeFileUnchecked()
    // This stream class is used as an interface for OutputStream
}

bool Ext4FileOutputStream::write(int byte) {
    throw IOException("write", m_path, 
        "Ext4FileOutputStream: Use Volume::writeFile() instead for complete file writes");
}

size_t Ext4FileOutputStream::write(const uint8_t* /*buf*/, size_t /*off*/, size_t /*len*/) {
    throw IOException("write", m_path, 
        "Ext4FileOutputStream: Use Volume::writeFile() instead for complete file writes");
}

void Ext4FileOutputStream::flush() {
    // No-op for this implementation
}

void Ext4FileOutputStream::close() {
    // No-op for this implementation
}
