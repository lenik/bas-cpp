#include "proc/DefAssets.hpp"

#include <bas/log/deflog.h>

extern "C" {
    
define_logger();

define_zip_assets(bas_cpp, bas_cpp_assets);

__attribute__((weak))
Volume* g_assets = bas_cpp_assets.get();

}