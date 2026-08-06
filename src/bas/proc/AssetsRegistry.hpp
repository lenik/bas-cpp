#ifndef ASSETSREGISTRY_HPP
#define ASSETSREGISTRY_HPP

#include "../volume/OverlayVolume.hpp"

#include <memory>

class AssetsRegistry {
  public:
    static std::unique_ptr<OverlayVolume>& instance();
    static void pushLayer(Volume* vol);
};

#endif // ASSETSREGISTRY_HPP
