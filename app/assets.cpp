#include <bas/log/uselog.h>
#include <bas/proc/AssetsRegistry.hpp>
#include <bas/volume/OverlayVolume.hpp>

#include <iostream>

void dumpLayers() {
    OverlayVolume* overlay = AssetsRegistry::instance().get();
    auto layers = overlay->layers();
    for (const auto& layer : layers) {
        // Layer <id>: <source> [ <root file count> ]
        auto node = layer->readDir("/");
        std::cout << "Layer " << layer->getId() << ": " << layer->getSource() //
                  << " [ " << node->childCount() << " ]"                      //
                  << std::endl;
    }
}

int main(int argc, char** argv) {
    argc--;
    argv++;

    ListOptions opts;
    if (argc > 0 && argv[0][0] == '-') {
        opts = ListOptions::parse(argv[0] + 1);
        argc--;
        argv++;
    }

    const char* path = "/";
    std::cout << "Path: " << path << std::endl;
    std::cout << "argc: " << argc << std::endl;

    if (argc >= 1) {
        for (int i = 0; i < argc; i++) {
            std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
            path = argv[i];
            OverlayVolume* overlay = AssetsRegistry::instance().get();
            std::cout << "Assets list:" << std::endl;
            AssetsRegistry::instance()->ls(path, opts);

            Volume* w = overlay->layerExists(path);
            std::cout << "Layer exists: " << (w ? w->getSource() : "no") << std::endl;

            std::string normalized = overlay->normalize(path);
            std::cout << "Normalized: " << normalized << std::endl;
            if (w) {
                bool isDir = w->isDirectory(normalized);
                std::cout << "Is directory: " << (isDir ? "yes" : "no") << std::endl;
            }
        }
    } else {
        dumpLayers();
    }
    return 0;
}
