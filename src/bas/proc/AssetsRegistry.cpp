#include "AssetsRegistry.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

std::unique_ptr<OverlayVolume>& AssetsRegistry::instance() {
    static std::unique_ptr<OverlayVolume> instance;
    return instance;
}

void AssetsRegistry::pushLayer(std::shared_ptr<Volume> vol) {
    if (!vol) {
        throw std::invalid_argument("Volume cannot be nullptr");
    }
    auto& inst = instance();
    if (!inst) {
        inst = std::make_unique<OverlayVolume>("", std::vector<std::shared_ptr<Volume>>{std::move(vol)});
    } else {
        inst->pushLayer(std::move(vol));
    }
}
