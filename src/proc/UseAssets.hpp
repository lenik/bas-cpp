#ifndef ASSETS_NAME
#error ASSETS_NAME is not defined
#endif

#include "../volume/Volume.hpp"
#include "../volume/OverlayVolume.hpp"

#include <memory>

#include <sys/cdefs.h>

#define __CONCAT_EVAL(x, y) __CONCAT(x, y)

#ifdef __cplusplus
extern "C" {
#endif

extern std::unique_ptr<Volume> __CONCAT_EVAL(ASSETS_NAME, _assets);

#ifdef __cplusplus
}
#endif
