#include "bas/volume/LocalVolume.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static void test_local_volume_file_resolves_host_path() {
    const fs::path hostPath = fs::absolute("/etc/hosts");
    if (!fs::exists(hostPath)) {
        return;
    }

    auto file = LocalVolume::file(hostPath.string());
    assert(file.getVolume() != nullptr);
    assert(file.exists());

    const std::string resolved = file.getLocalFile();
    assert(fs::equivalent(resolved, hostPath));

    const auto localVolume = std::static_pointer_cast<LocalVolume>(file.getVolume());
    const auto& mountPoint = localVolume->getMountPoint();
    assert(mountPoint.has_value());
    assert(resolved.rfind(*mountPoint, 0) == 0);
}

static void test_local_volume_at_caches_by_rootdir() {
    auto root = LocalVolume::at("/");
    auto again = LocalVolume::at("/");
    assert(root != nullptr);
    assert(root == again);
    assert(root == LocalVolume::at("//"));
}

int main() {
    test_local_volume_at_caches_by_rootdir();
    test_local_volume_file_resolves_host_path();
    std::cout << "LocalVolume tests passed\n";
    return 0;
}
