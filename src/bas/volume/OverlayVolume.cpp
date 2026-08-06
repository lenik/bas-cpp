#include "OverlayVolume.hpp"

#include "Volume.hpp"

#include "../io/IOException.hpp"
#include "../util/encoding.hpp"
#include "../util/uuid.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

OverlayVolume::OverlayVolume(std::string label, std::vector<std::shared_ptr<Volume>> layers)
    : m_layers(std::move(layers)), m_label(std::move(label)) {
    if (m_layers.empty()) {
        throw std::invalid_argument("OverlayVolume requires at least one volume layer");
    }
    for (const auto& layer : m_layers) {
        if (!layer) {
            throw std::invalid_argument("OverlayVolume layer cannot be nullptr");
        }
    }
    if (!m_label.empty()) {
        user_attrs = true;
        uint64_t hash = 0;
        for (const auto& layer : m_layers) {
            std::string uuid = layer->getUUID();
            hash = hash * 31 + std::hash<std::string>()(uuid);
        }
        m_serial = encoding::to_hex(hash);
        m_uuid = uuid::randomUUID(hash);
    }
}

std::string OverlayVolume::getClass() const { return m_class; }

std::string OverlayVolume::getUrl() const {
    if (!m_url.empty())
        return m_url;
    else
        return "overlay:" + getDeviceUrl();
}

std::string OverlayVolume::getDeviceUrl() const {
    if (!m_deviceUrl.empty())
        return m_deviceUrl;
    std::string out = "overlay:";
    int i = 1;
    for (const auto& v : m_layers) {
        if (i != 1)
            out += ",";
        out += std::to_string(i) + "=" + v->getDeviceUrl();
        i++;
    }
    return out;
}

VolumeType OverlayVolume::getType() const {
    if (m_volumeType.has_value())
        return *m_volumeType;
    if (m_layers.empty())
        return VolumeType::OTHER;
    VolumeType type = m_layers.front()->getType();
    for (const auto& layer : m_layers) {
        if (layer->getType() != type) {
            return VolumeType::OTHER;
        }
    }
    return type;
}

std::shared_ptr<Volume> OverlayVolume::bottomLayer() { return m_layers.front(); }
std::shared_ptr<Volume> OverlayVolume::topLayer() { return m_layers.back(); }
std::shared_ptr<const Volume> OverlayVolume::bottomLayer() const { return m_layers.front(); }
std::shared_ptr<const Volume> OverlayVolume::topLayer() const { return m_layers.back(); }

const std::vector<std::shared_ptr<Volume>>& OverlayVolume::getLayers() const { return m_layers; }
std::vector<std::shared_ptr<Volume>>& OverlayVolume::layers() { return m_layers; }

void OverlayVolume::pushLayer(std::shared_ptr<Volume> vol) {
    if (!vol) {
        throw std::invalid_argument("Volume cannot be nullptr");
    }
    if (std::find(m_layers.begin(), m_layers.end(), vol) != m_layers.end()) {
        throw std::invalid_argument("Volume already in overlay volume");
    }
    m_layers.push_back(std::move(vol));
}

void OverlayVolume::popLayer() {
    if (m_layers.size() <= 1) {
        throw std::invalid_argument("Overlay volume must have at least one layer");
    }
    m_layers.pop_back();
}

void OverlayVolume::removeLayer(const std::shared_ptr<Volume>& vol) {
    auto it = std::find(m_layers.begin(), m_layers.end(), vol);
    if (it == m_layers.end()) {
        throw std::invalid_argument("Volume not found in overlay volume");
    }
    if (m_layers.size() <= 1) {
        throw std::invalid_argument("Overlay volume must have at least one layer");
    }
    m_layers.erase(it);
}

std::string OverlayVolume::getDefaultLabel() const { return "Overlay Volume"; }

std::string OverlayVolume::getUUID() const {
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

std::string OverlayVolume::getSerial() const {
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

std::string OverlayVolume::getLabel() const {
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

std::optional<std::string> OverlayVolume::getLocalFile(std::string_view path) const {
    auto w = layerExists(path);
    if (!w)
        return std::nullopt;
    return w->getLocalFile(path);
}

std::shared_ptr<Volume> OverlayVolume::layerExists(std::string_view _path) const {
    std::string path(_path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        const auto& v = m_layers[i - 1];
        bool exists = v->exists(path);
        // std::cout << "layerExists: " << v->getSource() << " " << path << " exists: " << exists <<
        // std::endl;
        if (exists) {
            return v;
        }
    }
    return nullptr;
}

std::shared_ptr<Volume> OverlayVolume::layerForFile(std::string_view _path) const {
    std::string path(_path);
    for (size_t i = m_layers.size(); i > 0; --i) {
        const auto& v = m_layers[i - 1];
        if (v->isFile(path)) {
            return v;
        }
    }
    return nullptr;
}

bool OverlayVolume::exists(std::string_view path) const {
    for (const auto& layer : m_layers) {
        if (layer->exists(path)) {
            return true;
        }
    }
    return false;
}

bool OverlayVolume::isFile(std::string_view path) const {
    auto w = layerExists(path);
    return w && w->isFile(path);
}

bool OverlayVolume::isDirectory(std::string_view path) const {
    auto w = layerExists(path);
    return w && w->isDirectory(path);
}

bool OverlayVolume::stat(std::string_view path, DirNode* status) const {
    auto w = layerExists(path);
    if (!w) {
        return false;
    }
    return w->stat(path, status);
}

void OverlayVolume::readDir_inplace(DirNode& context, std::string_view path, bool recursive) {
    bool anyDir = false;
    for (const auto& layer : m_layers) {
        if (layer->isDirectory(path)) {
            anyDir = true;
            break;
        }
    }
    if (!anyDir) {
        throw IOException("readDir", path, "Path is not a directory");
    }

    for (auto& layer : m_layers) {
        if (!layer->exists(path)) {
            continue;
        }
        if (layer->isDirectory(path)) {
            layer->readDir_inplace(context, path, recursive);
        } else {
            DirNode stat;
            if (layer->stat(path, &stat)) {
                context.assign(stat);
                context.invalidateChildren();
            } else {
                context.setUnknownClear();
            }
        }
    }
}

std::unique_ptr<InputStream> OverlayVolume::newInputStream(std::string_view path) {
    auto v = layerForFile(path);
    if (!v) {
        throw IOException("newInputStream", path, "Path is not a readable file");
    }
    return v->newInputStream(path);
}

std::unique_ptr<OutputStream> OverlayVolume::newOutputStream(std::string_view path, bool append) {
    return m_layers.back()->newOutputStream(path, append);
}

std::unique_ptr<RandomInputStream> OverlayVolume::newRandomInputStream(std::string_view path) {
    auto v = layerForFile(path);
    if (!v) {
        throw IOException("newRandomInputStream", path, "Path is not a readable file");
    }
    return v->newRandomInputStream(path);
}

std::string OverlayVolume::getTempDir() {
    return m_layers.back()->getTempDir();
}

std::string OverlayVolume::createTempFile(std::string_view prefix, std::string_view suffix) {
    return m_layers.back()->createTempFile(prefix, suffix);
}

std::vector<uint8_t> OverlayVolume::readFileUnchecked(std::string_view path, int64_t off,
                                                      size_t len) {
    auto v = layerForFile(path);
    if (!v) {
        throw IOException("readFileUnchecked", path, "Path is not a readable file");
    }
    return v->readFile(path, off, len);
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
    auto srcLayer = layerForFile(src);
    if (!srcLayer) {
        throw IOException("copyFile", src, "Source file does not exist");
    }
    auto data = srcLayer->readFile(src);
    m_layers.back()->writeFile(dest, data);
}

void OverlayVolume::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    auto& top = m_layers.back();
    if (!top->exists(src) || !top->isFile(src)) {
        throw IOException("moveFile", src, "Source must exist on the top layer");
    }
    top->moveFileThrowsUnchecked(src, dest);
}

void OverlayVolume::renameFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    auto& top = m_layers.back();
    if (!top->exists(src)) {
        throw IOException("rename", src, "Source must exist on the top layer");
    }
    top->renameFileThrowsUnchecked(src, dest);
}
