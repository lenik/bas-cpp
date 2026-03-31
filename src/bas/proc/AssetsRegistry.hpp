#ifndef ASSETSREGISTRY_HPP
#define ASSETSREGISTRY_HPP

#include "../volume/OverlayVolume.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

class AssetsRegistry {
public:
    static std::unique_ptr<OverlayVolume>& instance() {
        static std::unique_ptr<OverlayVolume> instance;
        return instance;
    }
    static void pushLayer(Volume* vol) {
        if (vol == nullptr) {
            throw std::invalid_argument("Volume cannot be nullptr");
        }
        auto& inst = instance();
        if (!inst) {
            inst = std::make_unique<OverlayVolume>("", std::vector<Volume*>{vol});
        } else {
            inst->pushLayer(vol);
        }
    }
};

#endif // ASSETSREGISTRY_HPP
