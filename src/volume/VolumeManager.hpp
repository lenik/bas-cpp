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
    ~VolumeManager();
    
    /**
     * Add a volume to the manager.
     * @param volume The volume to add.
     * @return True if the volume was added, false if it was rejected by a transformer.
     */
    inline bool addVolume(std::unique_ptr<Volume> volume);
    inline void removeVolume(size_t index);
    inline void clear() { m_volumes.clear(); }
    
    inline size_t getVolumeCount() const { return m_volumes.size(); }
    inline Volume* getVolume(size_t index) const { return m_volumes[index].get(); }
    inline Volume* getDefaultVolume() const {
        if (m_volumes.empty())
            return nullptr;
        else
            return m_volumes[0].get();
    }
    
    inline const std::vector<std::unique_ptr<Volume>>& all() const { return m_volumes; }
    std::vector<Volume*> type(std::string_view type) const;

    /*
     * Discover and add local filesystem volumes (Linux: from /proc/self/mountinfo)
     *
     * rejected volumes are not added to the manager.
     */
    void addLocalVolumes();

    // apply a function to each volume
    void addTransformer(VolumeTransformer transformer) {
        m_transformers.push_back(transformer);
    }

};

#endif // VOLUMEMANAGER_H
