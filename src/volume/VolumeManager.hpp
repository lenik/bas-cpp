#ifndef VOLUMEMANAGER_H
#define VOLUMEMANAGER_H

#include "Volume.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

class VolumeManager {
private:
    std::vector<std::unique_ptr<Volume>> m_volumes;
    
public:
    VolumeManager();
    ~VolumeManager();
    
    inline void addVolume(std::unique_ptr<Volume> volume) { m_volumes.push_back(std::move(volume)); }
    inline void removeVolume(size_t index) {
        if (index >= m_volumes.size())
            throw std::out_of_range("Index out of range");
        m_volumes.erase(m_volumes.begin() + index);
    }

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

    // Discover and add local filesystem volumes (Linux: from /proc/self/mountinfo)
    void addLocalVolumes();

    // apply a function to each volume
    void apply(std::function<std::unique_ptr<Volume>(std::unique_ptr<Volume>)> transformer) {
        for (auto& volume : m_volumes) {
            volume = transformer(std::move(volume));
        }
    }

};

#endif // VOLUMEMANAGER_H
