#include "proc/AssetsRegistry.hpp"
#include "proc/DefAssets.hpp"

#include <bas/log/deflog.h>

define_zip_assets(bas_cpp, bas_cpp_assets);

extern "C" {

define_logger();

}

namespace {

struct BasCppAssetsRegistrar {
    BasCppAssetsRegistrar() {
        AssetsRegistry::pushLayer(bas_cpp_assets.get());
    }
} bas_cpp_assets_registrar;

}