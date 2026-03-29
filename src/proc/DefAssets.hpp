#ifndef ASSETS_NAME
#error ASSETS_NAME is not defined
#endif

#include "../volume/zip/MemoryZip.hpp"

#include <memory>

#include <sys/cdefs.h>

#define __CONCAT_EVAL(x, y) __CONCAT(x, y)

#define define_zip_assets(name, sym) \
    extern const unsigned char __CONCAT_EVAL(sym, _start)[]; \
    extern const unsigned char __CONCAT_EVAL(sym, _end)[]; \
    std::unique_ptr<MemoryZip> __CONCAT_EVAL(name, _assets) = \
        std::make_unique<MemoryZip>(__CONCAT_EVAL(sym, _start), __CONCAT_EVAL(sym, _end) - __CONCAT_EVAL(sym, _start))
