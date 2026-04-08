#include "ExfatFileOutputStream.hpp"
#include "../../io/IOException.hpp"

ExfatFileOutputStream::ExfatFileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath)), m_path(std::move(path)), m_append(append) {
    // Note: exFAT write operations are not yet implemented
    // This stream class is a placeholder for future implementation
}

bool ExfatFileOutputStream::write(int /*byte*/) {
    throw IOException("write", m_path, "exFAT write stream is not implemented yet");
}

size_t ExfatFileOutputStream::write(const uint8_t* /*buf*/, size_t /*off*/, size_t /*len*/) {
    throw IOException("write", m_path, "exFAT write stream is not implemented yet");
}

void ExfatFileOutputStream::flush() {
    // No-op for this implementation
}

void ExfatFileOutputStream::close() {
    // No-op for this implementation
}
