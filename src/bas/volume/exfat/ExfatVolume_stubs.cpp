#include "ExfatFileInputStream.hpp"
#include "ExfatFileOutputStream.hpp"
#include "ExfatVolume.hpp"

#include "../../io/IOException.hpp"

#include <algorithm>

std::unique_ptr<InputStream> ExfatVolume::newInputStream(std::string_view path) {
    const std::string normalized = normalizeArg(path);
    if (!ensurePathIndexed(normalized)) {
        throw IOException("newInputStream", std::string(path), "File not found");
    }
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end() || it->second.isDirectory) {
        throw IOException("newInputStream", std::string(path), "Not a regular file");
    }
    std::vector<uint32_t> chain;
    if (it->second.firstCluster >= 2 && it->second.size > 0) {
        chain = readClusterChain(it->second.firstCluster);
    }
    auto in = std::make_unique<ExfatFileInputStream>(m_device, std::move(chain),  //
                                                      static_cast<uint64_t>(m_clusterHeapOffset) * m_bytesPerSector,
                                                      m_clusterSize, it->second.size);
    if (!in->isOpen()) {
        throw IOException("newInputStream", std::string(path), "Failed to open input stream");
    }
    return in;
}

std::unique_ptr<OutputStream> ExfatVolume::newOutputStream(std::string_view path, bool append) {
    return std::make_unique<ExfatFileOutputStream>(this, std::string(path), append);
}

std::unique_ptr<RandomInputStream> ExfatVolume::newRandomInputStream(std::string_view path) {
    auto in = newInputStream(path);
    return std::unique_ptr<RandomInputStream>(dynamic_cast<RandomInputStream*>(in.release()));
}

std::string ExfatVolume::getTempDir() {
    throw IOException("getTempDir", "", "exFAT does not support temp dir");
}

std::string ExfatVolume::createTempFile(std::string_view /*prefix*/, std::string_view /*suffix*/) {
    throw IOException("createTempFile", "", "exFAT does not support temp file");
}

std::vector<uint8_t> ExfatVolume::readFileUnchecked(std::string_view path, int64_t off, size_t len) {
    const std::string normalized = normalizeArg(path);
    if (!ensurePathIndexed(normalized)) {
        throw IOException("readFile", std::string(path), "Path not found");
    }
    auto it = m_dirents.find(normalized);
    if (it == m_dirents.end() || it->second.isDirectory) {
        throw IOException("readFile", std::string(path), "Not a regular file");
    }

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(it->second.size));
    if (it->second.size > 0 && it->second.firstCluster >= 2) {
        auto stream = newInputStream(normalized);
        out = stream->readBytesUntilEOF();
        if (out.size() > it->second.size) out.resize(static_cast<size_t>(it->second.size));
    }

    int64_t start64 = (off >= 0) ? off : (static_cast<int64_t>(out.size()) + off + 1);
    if (start64 < 0) start64 = 0;
    size_t start = static_cast<size_t>(start64);
    if (start >= out.size()) return {};
    size_t end = out.size();
    if (len > 0) end = std::min(end, start + len);
    return std::vector<uint8_t>(out.begin() + start, out.begin() + end);
}

void ExfatVolume::readDir_inplace(DirNode& context, std::string_view path, bool recursive) {
    const std::string normalized = normalizeArg(path);
    if (!ensurePathIndexed(normalized)) {
        throw IOException("readDir", std::string(path), "Path not found");
    }
    auto pit = m_dirents.find(normalized);
    if (pit == m_dirents.end() || !pit->second.isDirectory || !ensureDirectoryParsed(normalized)) {
        throw IOException("readDir", std::string(path), "Not a directory");
    }
    for (const auto& kv : pit->second.children) {
        const std::string& name = kv.first;
        const Dirent* d = kv.second;
        auto node = std::make_unique<DirNode>();
        node->name = name;
        d->copyTo(*node);
        DirNode* childNode = context.putChild(std::move(node));
        if (recursive && d->isDirectory) {
            const std::string childPath = (normalized == "/") ? ("/" + name) : (normalized + "/" + name);
            readDir_inplace(*childNode, childPath, true);
        }
    }
    context.children_valid = true;
}
