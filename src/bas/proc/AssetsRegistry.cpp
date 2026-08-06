#include "AssetsRegistry.hpp"

#include <stdexcept>
#include <vector>

std::unique_ptr<OverlayVolume>& AssetsRegistry::instance() {
    static std::unique_ptr<OverlayVolume> instance;
    return instance;
}

void AssetsRegistry::pushLayer(Volume* vol) {
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
