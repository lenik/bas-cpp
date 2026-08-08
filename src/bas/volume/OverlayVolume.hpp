#ifndef OVERLAPVOLUME_H
#define OVERLAPVOLUME_H

#include "Volume.hpp"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * Stack of volumes: logical paths match; earlier layers are below, later layers override
 * for listing and reads. Mutations and writes go only to the last (top) layer.
 *
 * Layers are held by shared_ptr so OverlayVolume participates in their lifetime.
 */
class OverlayVolume : public Volume {
  public:
    explicit OverlayVolume(std::string label, std::vector<std::shared_ptr<Volume>> layers);

    template <typename... Ts>
    static std::unique_ptr<OverlayVolume> make(std::string label, Ts&&... vols) {
        static_assert(
            ((!std::is_pointer_v<std::remove_cv_t<std::remove_reference_t<Ts>>>) && ...),
            "OverlayVolume::make expects shared_ptr or unique_ptr layers, not raw pointers");
        std::vector<std::shared_ptr<Volume>> vec;
        vec.reserve(sizeof...(Ts));
        (vec.push_back(std::shared_ptr<Volume>(std::forward<Ts>(vols))), ...);
        return std::make_unique<OverlayVolume>(std::move(label), std::move(vec));
    }

    std::shared_ptr<Volume> bottomLayer();
    std::shared_ptr<Volume> topLayer();
    std::shared_ptr<const Volume> bottomLayer() const;
    std::shared_ptr<const Volume> topLayer() const;

    const std::vector<std::shared_ptr<Volume>>& getLayers() const;
    std::vector<std::shared_ptr<Volume>>& layers();

    std::shared_ptr<Volume> layerExists(std::string_view path) const;
    std::shared_ptr<Volume> layerForFile(std::string_view path) const;

    void pushLayer(std::shared_ptr<Volume> vol);
    void popLayer();
    void removeLayer(const std::shared_ptr<Volume>& vol);

    std::string getClass() const override;
    std::string getUrl() const override;
    std::string getDeviceUrl() const override;
    VolumeType getType() const override;

    void setClass(std::string_view c) { m_class = std::string(c); }
    void setUrl(std::string_view url) { m_url = std::string(url); }
    void setDeviceUrl(std::string_view url) { m_deviceUrl = std::string(url); }
    void setVolumeType(VolumeType t) { m_volumeType = t; }

    std::string readUuid() override;
    std::string readLabel() override;

    bool writeUuid(std::string_view s) override;
    bool writeLabel(std::string_view label) override;

    std::optional<std::string> getLocalFile(std::string_view path) const override;

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, DirNode* status) const override;

    void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path,
                                                  bool append = false) override;
    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.",
                               std::string_view suffix = "") override;

  protected:
    std::string getDefaultLabel() const override;

    std::vector<uint8_t> readFileUnchecked(std::string_view path, int64_t off = 0,
                                           size_t len = 0) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;

    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view src, std::string_view dest) override;

  private:
    std::vector<std::shared_ptr<Volume>> m_layers;
    std::string m_class = "overlay";
    std::string m_url = "overlay:";
    std::string m_deviceUrl = "none";
    std::optional<VolumeType> m_volumeType = std::nullopt;

    bool user_attrs = true;
    std::string m_uuid;
    std::string m_label;
};

#endif // OVERLAPVOLUME_H
