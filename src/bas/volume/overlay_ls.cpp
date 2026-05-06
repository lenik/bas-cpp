#include "overlay_ls.hpp"

#include <bas/proc/AssetsRegistry.hpp>
#include <bas/volume/OverlayVolume.hpp>

#include <iostream>

#include <bas/log/uselog.h>

class OverlayUtils {
  public:
    OverlayUtils(OverlayVolume* overlay) : m_overlay(overlay) {}

    void list_layers();
    void list_files(const std::string& path, const ListOptions& opts);
    void diagnose_path(const std::string& path);

  private:
    OverlayVolume* m_overlay;
};

void OverlayUtils::list_layers() {
    auto layers = m_overlay->layers();
    for (const auto& layer : layers) {
        // Layer <url>: <device url> [ <root file count> ]
        std::string url = layer->getUrl();
        std::string deviceUrl = layer->getDeviceUrl();
        auto root = layer->readDir("/");
        int rootFileCount = root->childCount();
        std::cout << "Layer " << url << " (" << deviceUrl << ")" //
                  << ": " << rootFileCount << " root files" << std::endl;
    }
}

void OverlayUtils::list_files(const std::string& path, const ListOptions& opts) {
    m_overlay->ls(path, opts);
}

void OverlayUtils::diagnose_path(const std::string& path) {
    std::string normalized = m_overlay->normalize(path);
    std::cout << "Normalized Path: " << normalized << std::endl;

    Volume* w = m_overlay->layerExists(path);
    if (w) {
        std::cout << "In Layer: " << w->getUrl() << " (" << w->getDeviceUrl() << ")" << std::endl;
        bool isDir = w->isDirectory(normalized);
        std::cout << "Is directory: " << (isDir ? "yes" : "no") << std::endl;
    } else {
        std::cout << "Not in any layer" << std::endl;
    }
}

int overlay_ls(OverlayVolume* overlay, int argc, char** argv) {
    OverlayUtils utils(overlay);
    if (argc <= 1) {
        utils.list_layers();
        return 0;
    }

    std::optional<ListOptions> opts = ListOptions::parse(argc, argv);
    if (!opts) {
        return 1;
    }
    for (int i = 0; i < argc; i++) {
        std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
        char* arg = argv[i];
        std::cout << "Directory Listing: " << arg << ":" << std::endl;
        utils.list_files(arg, *opts);

        std::cout << std::endl;
        std::cout << "Diagnosing path: " << arg << std::endl;
        utils.diagnose_path(arg);
    }
    return 0;
}
