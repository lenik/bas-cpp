#ifndef ASSETS_NAME
#error ASSETS_NAME is not defined
#endif

#include "../volume/zip/MemoryZip.hpp"

#include <memory>

#include <sys/cdefs.h>

#define __CONCAT_EVAL(x, y) __CONCAT(x, y)

#define define_zip_assets(name) \
    extern const unsigned char __CONCAT_EVAL(name, _assets_data_start)[]; \
    extern const unsigned char __CONCAT_EVAL(name, _assets_data_end)[]; \
    std::unique_ptr<MemoryZip> __CONCAT_EVAL(name, _assets) = \
        std::make_unique<MemoryZip>(__CONCAT_EVAL(name, _assets_data_start), __CONCAT_EVAL(name, _assets_data_end) - __CONCAT_EVAL(name, _assets_data_start))
