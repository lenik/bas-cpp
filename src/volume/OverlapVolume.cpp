#include "OverlapVolume.hpp"

#include "../io/IOException.hpp"

#include <map>
#include <stdexcept>
#include <utility>

static void eraseSubtreeKeys(std::map<std::string, std::unique_ptr<FileStatus>>& m,
                             const std::string& prefix) {
    auto it = m.lower_bound(prefix);
    while (it != m.end() && it->first.size() >= prefix.size() &&
           it->first.compare(0, prefix.size(), prefix) == 0) {
        it = m.erase(it);
    }
}

static void mergeDirEntry(std::map<std::string, std::unique_ptr<FileStatus>>& m,
                          std::unique_ptr<FileStatus> st) {
    const std::string& key = st->name;
    // A non-directory at `key` masks any directory tree below; drop stale descendants.
    // Two directories at `key` merge: keep existing `key/...` from lower layers unless
    // overwritten by this layer's entries (merged later in the same pass).
    if (!st->isDirectory()) {
        eraseSubtreeKeys(m, key + "/");
    }
    m[key] = std::move(st);
}

OverlapVolume::OverlapVolume(std::vector<Volume*> layers)
    : m_layers(std::move(layers)) {
    if (m_layers.empty()) {
        throw std::invalid_argument("OverlapVolume requires at least one volume layer");
    }
}

std::string OverlapVolume::getDefaultLabel() const { return "Overlap Volume"; }

std::string OverlapVolume::getUUID() { 
    if (user_attrs) {
        return m_uuid;
    } else {
        return Volume::getUUID();
    }
}

void OverlapVolume::setUUID(std::string_view u) {
    if (user_attrs) {
        m_uuid = std::string(u);
    } else {
        Volume::setUUIDForced(u);
    }
}

std::string OverlapVolume::getSerial() { 
    if (user_attrs) {
        return m_serial;
    } else {
        return Volume::getSerial();
    }
}

void OverlapVolume::setSerial(std::string_view s) {
    if (user_attrs) {
        m_serial = std::string(s);
    } else {
        Volume::setSerialForced(s);
    }
}

std::string OverlapVolume::getLabel() { 
    if (user_attrs) {
        return m_label;
    } else {
        return Volume::getLabel();
    }
}

void OverlapVolume::setLabel(std::string_view label) {
    if (user_attrs) {
        m_label = std::string(label);
    } else {
        Volume::setLabel(label);
    }
}

std::string OverlapVolume::getLocalFile(std::string_view path) const {
    Volume* w = layerExists(path);
    return w ? w->getLocalFile(path) : std::string();
}

Volume* OverlapVolume::layerExists(std::string_view path) const {
    std::string p = normalize(path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        Volume* v = m_layers[i - 1];
        if (v->exists(p)) {
            return v;
        }
    }
    return nullptr;
}

Volume* OverlapVolume::layerForFile(std::string_view path) const {
    std::string p = normalize(path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        Volume* v = m_layers[i - 1];
        if (v->exists(p) && v->isFile(p)) {
            return v;
        }
    }
    return nullptr;
}

bool OverlapVolume::exists(std::string_view path) const {
    std::string p = normalize(path);
    for (const auto& layer : m_layers) {
        if (layer->exists(p)) {
            return true;
        }
    }
    return false;
}

bool OverlapVolume::isFile(std::string_view path) const {
    Volume* w = layerExists(path);
    return w && w->isFile(normalize(path));
}

bool OverlapVolume::isDirectory(std::string_view path) const {
    Volume* w = layerExists(path);
    return w && w->isDirectory(normalize(path));
}

bool OverlapVolume::stat(std::string_view path, FileStatus* status) const {
    Volume* w = layerExists(path);
    if (!w) {
        return false;
    }
    return w->stat(normalize(path), status);
}

void OverlapVolume::readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list,
                                    std::string_view path, bool recursive) {
    std::string norm = normalize(path);
    bool anyDir = false;
    for (const auto& layer : m_layers) {
        if (layer->isDirectory(norm)) {
            anyDir = true;
            break;
        }
    }
    if (!anyDir) {
        throw IOException("readDir", norm, "Path is not a directory");
    }

    std::map<std::string, std::unique_ptr<FileStatus>> merged;
    for (auto& layer : m_layers) {
        if (!layer->isDirectory(norm)) {
            continue;
        }
        std::vector<std::unique_ptr<FileStatus>> sub;
        layer->readDir_inplace(sub, norm, recursive);
        for (auto& e : sub) {
            mergeDirEntry(merged, std::move(e));
        }
    }

    list.clear();
    list.reserve(merged.size());
    for (auto& kv : merged) {
        list.push_back(std::move(kv.second));
    }
}

std::unique_ptr<InputStream> OverlapVolume::newInputStream(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("newInputStream", std::string(normalize(path)), "Path is not a readable file");
    }
    return v->newInputStream(path);
}

std::unique_ptr<OutputStream> OverlapVolume::newOutputStream(std::string_view path, bool append) {
    return m_layers.back()->newOutputStream(path, append);
}

std::unique_ptr<RandomInputStream> OverlapVolume::newRandomInputStream(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("newRandomInputStream", std::string(normalize(path)),
                          "Path is not a readable file");
    }
    return v->newRandomInputStream(path);
}

std::vector<uint8_t> OverlapVolume::readFile(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("readFile", std::string(normalize(path)), "Path is not a readable file");
    }
    return v->readFile(path);
}

void OverlapVolume::writeFile(std::string_view path, const std::vector<uint8_t>& data) {
    m_layers.back()->writeFile(path, data);
}

std::string OverlapVolume::getTempDir() { return m_layers.back()->getTempDir(); }

std::string OverlapVolume::createTempFile(std::string_view prefix, std::string_view suffix) {
    return m_layers.back()->createTempFile(prefix, suffix);
}

void OverlapVolume::createDirectoryThrowsUnchecked(std::string_view path) {
    m_layers.back()->createDirectoryThrows(path);
}

void OverlapVolume::removeDirectoryThrowsUnchecked(std::string_view path) {
    m_layers.back()->removeDirectoryThrows(path);
}

void OverlapVolume::removeFileThrowsUnchecked(std::string_view path) {
    m_layers.back()->removeFileThrows(path);
}

void OverlapVolume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    std::string s = normalize(src);
    Volume* srcLayer = layerForFile(s);
    if (!srcLayer) {
        throw IOException("copyFile", s, "Source file does not exist");
    }
    std::vector<uint8_t> data = srcLayer->readFile(s);
    m_layers.back()->writeFile(normalize(dest), data);
}

void OverlapVolume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    Volume* top = m_layers.back();
    std::string s = normalize(src);
    if (!top->exists(s) || !top->isFile(s)) {
        throw IOException("moveFile", s, "Source must exist on the top layer");
    }
    top->moveFile(src, dest);
}

void OverlapVolume::renameFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    Volume* top = m_layers.back();
    std::string s = normalize(src);
    if (!top->exists(s)) {
        throw IOException("rename", s, "Source must exist on the top layer");
    }
    top->rename(src, dest);
}
