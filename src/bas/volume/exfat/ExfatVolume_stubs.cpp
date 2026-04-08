#include "ExfatVolume.hpp"
#include "../../io/IOException.hpp"

// Stub implementations for unimplemented methods

std::unique_ptr<InputStream> ExfatVolume::newInputStream(std::string_view /*path*/) {
    throw IOException("newInputStream", "", "exFAT newInputStream not implemented");
}

std::unique_ptr<OutputStream> ExfatVolume::newOutputStream(std::string_view /*path*/, bool /*append*/) {
    throw IOException("newOutputStream", "", "exFAT newOutputStream not implemented");
}

std::unique_ptr<RandomInputStream> ExfatVolume::newRandomInputStream(std::string_view /*path*/) {
    throw IOException("newRandomInputStream", "", "exFAT newRandomInputStream not implemented");
}

std::string ExfatVolume::getTempDir() {
    throw IOException("getTempDir", "", "exFAT getTempDir not implemented");
}

std::string ExfatVolume::createTempFile(std::string_view /*prefix*/, std::string_view /*suffix*/) {
    throw IOException("createTempFile", "", "exFAT createTempFile not implemented");
}

std::vector<uint8_t> ExfatVolume::readFileUnchecked(std::string_view /*path*/, int64_t /*off*/, size_t /*len*/) {
    throw IOException("readFileUnchecked", "", "exFAT readFileUnchecked not implemented");
}

void ExfatVolume::readDir_inplace(DirNode& /*context*/, std::string_view /*path*/, bool /*recursive*/) {
    throw IOException("readDir_inplace", "", "exFAT readDir_inplace not implemented");
}
