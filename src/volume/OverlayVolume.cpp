#include "OverlayVolume.hpp"

#include "../io/IOException.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
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

template <typename T> std::string toHex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

std::string toUUID(uint64_t random_seed) {
    // 1. Initialize a better random engine with the seed
    std::mt19937_64 gen(random_seed);
    std::uniform_int_distribution<uint64_t> dis(0, 0xFFFFFFFFFFFFFFFF);

    // 2. Generate two 64-bit random numbers (128 bits total)
    uint64_t high = dis(gen);
    uint64_t low = dis(gen);

    // 3. Set Version 4 (0100) and Variant (10xx) bits
    // Version 4: set the 4 bits starting at bit 48 of 'high' to 0100
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Variant: set the 2 bits starting at bit 60 of 'low' to 10
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    // 4. Format into canonical 8-4-4-4-12 string
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << (uint32_t)(high >> 32) << "-"
       << std::setw(4) << (uint16_t)(high >> 16) << "-" << std::setw(4) << (uint16_t)high << "-"
       << std::setw(4) << (uint16_t)(low >> 48) << "-" << std::setw(12)
       << (low & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

OverlayVolume::OverlayVolume(std::string label, std::vector<Volume*> layers)
    : m_layers(std::move(layers)), m_label(label) {
    if (m_layers.empty()) {
        throw std::invalid_argument("OverlayVolume requires at least one volume layer");
    }
    if (!m_label.empty()) {
        user_attrs = true;
        uint64_t hash = 0;
        for (const auto& layer : m_layers) {
            std::string uuid = layer->getUUID();
            hash = hash * 31 + std::hash<std::string>()(uuid);
        }
        m_serial = toHex(hash);
        m_uuid = toUUID(hash);
    }
}

std::string OverlayVolume::getClass() const { return m_class; }
std::string OverlayVolume::getId() const { return m_id; }
VolumeType OverlayVolume::getType() const { return m_volumeType; }
std::string OverlayVolume::getSource() const {
    std::string out = "overlay<";
    for (Volume* v : m_layers) {
        out += "; ";
        out += v->getSource();
    }
    out += ">";
    return out;
}

Volume* OverlayVolume::bottomLayer() { return m_layers.front(); }
Volume* OverlayVolume::topLayer() { return m_layers.back(); }
const Volume* OverlayVolume::bottomLayer() const { return m_layers.front(); }
const Volume* OverlayVolume::topLayer() const { return m_layers.back(); }

const std::vector<Volume*>& OverlayVolume::getLayers() const { return m_layers; }
std::vector<Volume*>& OverlayVolume::layers() { return m_layers; }

void OverlayVolume::pushLayer(Volume* vol) {
    if (vol == nullptr) {
        throw std::invalid_argument("Volume cannot be nullptr");
    }
    if (std::find(m_layers.begin(), m_layers.end(), vol) != m_layers.end()) {
        throw std::invalid_argument("Volume already in overlay volume");
    }
    m_layers.push_back(vol);
}

void OverlayVolume::popLayer() {
    if (m_layers.empty()) {
        throw std::invalid_argument("Overlay volume must have at least one layer");
    }
    m_layers.pop_back();
}

void OverlayVolume::removeLayer(Volume* vol) {
    if (std::find(m_layers.begin(), m_layers.end(), vol) == m_layers.end()) {
        throw std::invalid_argument("Volume not found in overlay volume");
    }
    m_layers.erase(std::remove(m_layers.begin(), m_layers.end(), vol), m_layers.end());
    if (m_layers.empty()) {
        throw std::invalid_argument("Overlay volume must have at least one layer");
    }
}

std::string OverlayVolume::getDefaultLabel() const { return "Overlay Volume"; }

std::string OverlayVolume::getUUID() {
    if (user_attrs) {
        return m_uuid;
    } else {
        return Volume::getUUID();
    }
}

void OverlayVolume::setUUID(std::string_view u) {
    if (user_attrs) {
        m_uuid = std::string(u);
    } else {
        Volume::setUUIDForced(u);
    }
}

std::string OverlayVolume::getSerial() {
    if (user_attrs) {
        return m_serial;
    } else {
        return Volume::getSerial();
    }
}

void OverlayVolume::setSerial(std::string_view s) {
    if (user_attrs) {
        m_serial = std::string(s);
    } else {
        Volume::setSerialForced(s);
    }
}

std::string OverlayVolume::getLabel() {
    if (user_attrs) {
        return m_label;
    } else {
        return Volume::getLabel();
    }
}

void OverlayVolume::setLabel(std::string_view label) {
    if (user_attrs) {
        m_label = std::string(label);
    } else {
        Volume::setLabel(label);
    }
}

std::string OverlayVolume::getLocalFile(std::string_view path) const {
    Volume* w = layerExists(path);
    return w ? w->getLocalFile(path) : std::string();
}

Volume* OverlayVolume::layerExists(std::string_view _path) const {
    std::string path(_path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        Volume* v = m_layers[i - 1];
        bool exists = v->exists(path);
        // std::cout << "layerExists: " << v->getSource() << " " << path << " exists: " << exists << std::endl;
        if (exists) {
            return v;
        }
    }
    return nullptr;
}

Volume* OverlayVolume::layerForFile(std::string_view _path) const {
    std::string path(_path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        Volume* v = m_layers[i - 1];
        if (v->isFile(path)) {
            return v;
        }
    }
    return nullptr;
}

bool OverlayVolume::exists(std::string_view path) const {
    std::string p = normalize(path);
    for (const auto& layer : m_layers) {
        if (layer->exists(p)) {
            return true;
        }
    }
    return false;
}

bool OverlayVolume::isFile(std::string_view path) const {
    Volume* w = layerExists(path);
    return w && w->isFile(normalize(path));
}

bool OverlayVolume::isDirectory(std::string_view path) const {
    Volume* w = layerExists(path);
    return w && w->isDirectory(normalize(path));
}

bool OverlayVolume::stat(std::string_view path, FileStatus* status) const {
    Volume* w = layerExists(path);
    if (!w) {
        return false;
    }
    return w->stat(normalize(path), status);
}

void OverlayVolume::readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list,
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

std::unique_ptr<InputStream> OverlayVolume::newInputStream(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("newInputStream", std::string(normalize(path)),
                          "Path is not a readable file");
    }
    return v->newInputStream(path);
}

std::unique_ptr<OutputStream> OverlayVolume::newOutputStream(std::string_view path, bool append) {
    return m_layers.back()->newOutputStream(path, append);
}

std::unique_ptr<RandomInputStream> OverlayVolume::newRandomInputStream(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("newRandomInputStream", std::string(normalize(path)),
                          "Path is not a readable file");
    }
    return v->newRandomInputStream(path);
}

std::string OverlayVolume::getTempDir() { return m_layers.back()->getTempDir(); }

std::string OverlayVolume::createTempFile(std::string_view prefix, std::string_view suffix) {
    return m_layers.back()->createTempFile(prefix, suffix);
}

std::vector<uint8_t> OverlayVolume::readFileUnchecked(std::string_view path) {
    Volume* v = layerForFile(path);
    if (!v) {
        throw IOException("readFile", std::string(normalize(path)), "Path is not a readable file");
    }
    return v->readFile(path);
}

void OverlayVolume::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    m_layers.back()->writeFile(path, data);
}

void OverlayVolume::createDirectoryThrowsUnchecked(std::string_view path) {
    m_layers.back()->createDirectoryThrows(path);
}

void OverlayVolume::removeDirectoryThrowsUnchecked(std::string_view path) {
    m_layers.back()->removeDirectoryThrows(path);
}

void OverlayVolume::removeFileThrowsUnchecked(std::string_view path) {
    m_layers.back()->removeFileThrows(path);
}

void OverlayVolume::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    std::string s = normalize(src);
    Volume* srcLayer = layerForFile(s);
    if (!srcLayer) {
        throw IOException("copyFile", s, "Source file does not exist");
    }
    std::vector<uint8_t> data = srcLayer->readFile(s);
    m_layers.back()->writeFile(normalize(dest), data);
}

void OverlayVolume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    Volume* top = m_layers.back();
    std::string s = normalize(src);
    if (!top->exists(s) || !top->isFile(s)) {
        throw IOException("moveFile", s, "Source must exist on the top layer");
    }
    top->moveFile(src, dest);
}

void OverlayVolume::renameFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    Volume* top = m_layers.back();
    std::string s = normalize(src);
    if (!top->exists(s)) {
        throw IOException("rename", s, "Source must exist on the top layer");
    }
    top->rename(src, dest);
}
