#include <bas/proc/AssetsRegistry.hpp>
#include <bas/volume/overlay_ls.hpp>

int main(int argc, char** argv) {
    auto vol = AssetsRegistry::instance().get();
    return overlay_ls(vol, argc, argv);
}
