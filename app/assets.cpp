#include "proc/UseAssets.hpp"
#include "proc/AssetsRegistry.hpp"

#include <iostream>

int main(int argc, char** argv) {
    argc--;
    argv++;

    const char* options = NULL;
    if (argc > 0 && argv[0][0] == '-') {
        options = argv[0] + 1;
        argc--;
        argv++;
    }

    const char* path = "/";
    if (argc > 0) {
        path = argv[0];
        argc--;
        argv++;
    }

    if (options) {
        std::cout << "Assets list:" << std::endl;
        AssetsRegistry::instance()->ls(options, path);
    } else {
        std::cout << "Assets tree:" << std::endl;
        AssetsRegistry::instance()->tree(path);
    }
    return 0;
}
