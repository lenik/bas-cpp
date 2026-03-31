#include "LocalProtocol.hpp"

#include "URL.hpp"

#include "../io/LocalInputStream.hpp"
#include "../io/LocalOutputStream.hpp"

#include <memory>

std::unique_ptr<InputStream> LocalProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "file") return nullptr;
    std::string path = url.getPath();
    if (path.empty()) return nullptr;
    return std::make_unique<LocalInputStream>(path);
}

std::unique_ptr<RandomInputStream> LocalProtocol::newRandomInputStream(const URL& url) {
    if (url.getScheme() != "file") return nullptr;
    std::string path = url.getPath();
    if (path.empty()) return nullptr;
    return std::make_unique<LocalInputStream>(path);
}

std::unique_ptr<OutputStream> LocalProtocol::newOutputStream(const URL& url, bool append) {
    if (url.getScheme() != "file") return nullptr;
    std::string path = url.getPath();
    if (path.empty()) return nullptr;
    return std::make_unique<LocalOutputStream>(path, append);
}
