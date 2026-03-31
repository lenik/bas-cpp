#include "bas/proc/UseAssets.hpp"
#include "bas/proc/AssetsRegistry.hpp"

#include <iostream>

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
    if (argc > 0) {
        path = argv[0];
        argc--;
        argv++;
    }

    std::cout << "Assets list:" << std::endl;
    AssetsRegistry::instance()->ls(path, opts);
    return 0;
}
