#ifndef VOLUMEMANAGER_H
#define VOLUMEMANAGER_H

#include "Volume.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

class VolumeManager {

    using VolumeTransformer = std::function<std::unique_ptr<Volume>(std::unique_ptr<Volume>)>;

    std::vector<std::unique_ptr<Volume>> m_volumes;
    std::vector<VolumeTransformer> m_transformers;
    
public:
    VolumeManager();
    virtual ~VolumeManager();
    
    /**
     * Add a volume to the manager.
     * @param volume The volume to add.
     * @return True if the volume was added, false if it was rejected by a transformer.
     */
    bool addVolume(std::unique_ptr<Volume> volume);
    void removeVolume(size_t index);
    void clear();
    
    size_t getVolumeCount() const;
    Volume* getVolume(size_t index) const;
    Volume* getDefaultVolume() const;
    
    const std::vector<std::unique_ptr<Volume>>& all() const;
    std::vector<Volume*> type(std::string_view type) const;

    /**
     * Resolve a virtual URI into a file handle.
     *
     * URI format:
     *   vol://<index><path>
     * where <path> always starts with '/'.
     *
     * Returns std::nullopt on invalid URI or unknown volume index.
     */
    std::optional<VolumeFile> resolveUri(std::string_view uri) const;

    /**
     * Discover and add local filesystem volumes (Linux: from /proc/self/mountinfo).
     * @param includeSymbols when false, skip overlay mounts; bind mounts are always included.
     * @param excludeReadOnly when true, skip read-only mounts.
     */
    void addLocalVolumes(bool includeSymbols = false, bool excludeReadOnly = false);

    // apply a function to each volume
    void addTransformer(VolumeTransformer transformer);

};

#endif // VOLUMEMANAGER_H
