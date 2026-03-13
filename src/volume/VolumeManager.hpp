#ifndef VOLUMEMANAGER_H
#define VOLUMEMANAGER_H

#include "Volume.hpp"

#include <functional>
#include <memory>
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

    /*
     * Discover and add local filesystem volumes (Linux: from /proc/self/mountinfo)
     *
     * rejected volumes are not added to the manager.
     */
    void addLocalVolumes();

    // apply a function to each volume
    void addTransformer(VolumeTransformer transformer);

};

#endif // VOLUMEMANAGER_H
