#ifndef ASSETS_NAME
#error ASSETS_NAME is not defined
#endif

#include "../volume/dev/MemDevice.hpp"
#include "../volume/zip/ZipVolume.hpp"

#include <memory>

#include <sys/cdefs.h>

#define __CONCAT_EVAL(x, y) __CONCAT(x, y)

#define define_zip_assets(name, sym) \
    extern const unsigned char __CONCAT_EVAL(sym, _start)[]; \
    extern const unsigned char __CONCAT_EVAL(sym, _end)[]; \
    std::unique_ptr<ZipVolume> __CONCAT_EVAL(name, _assets) = \
        std::make_unique<ZipVolume>( \
            std::make_shared<MemDevice>(__CONCAT_EVAL(sym, _start), \
                                         static_cast<size_t>(__CONCAT_EVAL(sym, _end) - __CONCAT_EVAL(sym, _start)), \
                                         #name))

#if defined(_MSC_VER)
    #define INITIALIZER(f) \
        static void f(void); \
        struct f##_struct { f##_struct() { f(); } }; \
        static f##_struct f##_obj; \
        static void f(void)
#else
    #define INITIALIZER(f) \
        static void f(void) __attribute__((constructor)); \
        static void f(void)
#endif
