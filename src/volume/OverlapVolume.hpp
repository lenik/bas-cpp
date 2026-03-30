#ifndef OVERLAPVOLUME_H
#define OVERLAPVOLUME_H

#include "Volume.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

/**
 * Stack of volumes: logical paths match; earlier layers are below, later layers override
 * for listing and reads. Mutations and writes go only to the last (top) layer.
 */
class OverlapVolume : public Volume {
public:
    explicit OverlapVolume(std::string label, std::vector<Volume*> layers);

    template<typename... Ts>
    static std::unique_ptr<OverlapVolume> make(std::string label, Ts&&... vols) {
        static_assert((std::is_convertible_v<Ts*, Volume*> && ...));
        std::vector<Volume*> vec;
        vec.reserve(sizeof...(Ts));
        (vec.push_back(vols), ...);
        return std::make_unique<OverlapVolume>(label, std::move(vec));
    }

    void pushLayer(Volume* vol);
    void popLayer();
    void removeLayer(Volume* vol);

    Volume* bottomLayer() { return m_layers.front(); }
    Volume* topLayer() { return m_layers.back(); }
    const Volume* bottomLayer() const { return m_layers.front(); }
    const Volume* topLayer() const { return m_layers.back(); }

    std::string getClass() const override { return m_class; }
    std::string getId() const override { return m_id; }
    VolumeType getType() const override { return m_volumeType; }
    
    void setClass(std::string_view c) { m_class = std::string(c); }
    void setId(std::string_view id) { m_id = std::string(id); }
    void setVolumeType(VolumeType t) { m_volumeType = t; }
    
    std::string getUUID() override;
    std::string getSerial() override;
    std::string getLabel() override;
    
    void setUUID(std::string_view u);
    void setSerial(std::string_view s);
    void setLabel(std::string_view label) override;

    std::string getLocalFile(std::string_view path) const override;

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, FileStatus* status) const override;

    void readDir_inplace(std::vector<std::unique_ptr<FileStatus>>& list, std::string_view path,
                         bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path, bool append = false) override;
    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.", std::string_view suffix = "") override;

protected:
    std::string getDefaultLabel() const override;

    std::vector<uint8_t> readFileUnchecked(std::string_view path) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;

    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view src, std::string_view dest) override;

private:
    Volume* layerExists(std::string_view path) const;
    Volume* layerForFile(std::string_view path) const;

    std::vector<Volume*> m_layers;
    std::string m_class = "overlap";
    std::string m_id;
    VolumeType m_volumeType = VolumeType::OTHER;
    
    bool user_attrs = true;
    std::string m_uuid;
    std::string m_serial;
    std::string m_label;
};

#endif // OVERLAPVOLUME_H
