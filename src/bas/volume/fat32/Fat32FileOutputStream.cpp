#include "Fat32FileOutputStream.hpp"

#include "../../io/IOException.hpp"

Fat32FileOutputStream::Fat32FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath)), m_path(std::move(path)), m_append(append) {
    // Note: This is a placeholder implementation
    // The actual write operations are handled by Fat32Volume::writeFileUnchecked()
    // This stream class is used as an interface for OutputStream
}

bool Fat32FileOutputStream::write(int byte) {
    throw IOException("write", m_path, 
        "Fat32FileOutputStream: Use Volume::writeFile() instead for complete file writes");
}

size_t Fat32FileOutputStream::write(const uint8_t* /*buf*/, size_t /*off*/, size_t /*len*/) {
    throw IOException("write", m_path, 
        "Fat32FileOutputStream: Use Volume::writeFile() instead for complete file writes");
}

void Fat32FileOutputStream::flush() {
    // No-op for this implementation
}

void Fat32FileOutputStream::close() {
    // No-op for this implementation
}
