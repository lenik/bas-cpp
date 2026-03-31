#include "Ext4FileOutputStream.hpp"

#include "../../io/IOException.hpp"

Ext4FileOutputStream::Ext4FileOutputStream(std::string imagePath, std::string path, bool append)
    : m_imagePath(std::move(imagePath)), m_path(std::move(path)), m_append(append) {}

bool Ext4FileOutputStream::write(int /*byte*/) {
    throw IOException("write", m_path, "EXT write stream is not implemented yet");
}

size_t Ext4FileOutputStream::write(const uint8_t* /*buf*/, size_t /*off*/, size_t /*len*/) {
    throw IOException("write", m_path, "EXT write stream is not implemented yet");
}

void Ext4FileOutputStream::flush() {}

void Ext4FileOutputStream::close() {}
