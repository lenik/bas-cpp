#include "Fat32FileOutputStream.hpp"

#include "../../io/IOException.hpp"

Fat32FileOutputStream::Fat32FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath)), m_path(std::move(path)), m_append(append) {}

bool Fat32FileOutputStream::write(int /*byte*/) {
    throw IOException("write", m_path, "FAT32 write stream is not implemented yet");
}

size_t Fat32FileOutputStream::write(const uint8_t* /*buf*/, size_t /*off*/, size_t /*len*/) {
    throw IOException("write", m_path, "FAT32 write stream is not implemented yet");
}

void Fat32FileOutputStream::flush() {}

void Fat32FileOutputStream::close() {}
