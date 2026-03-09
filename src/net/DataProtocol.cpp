#include "DataProtocol.hpp"

#include "DataUriInputStream.hpp"
#include "URL.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"

#include <memory>

std::unique_ptr<InputStream> DataProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "data") return nullptr;
    std::string path = url.getPath();
    if (path.empty()) return nullptr;
    auto stream = std::make_unique<DataUriInputStream>("data:" + path, true);
    if (!stream->ok()) return nullptr;
    return stream;
}

std::unique_ptr<OutputStream> DataProtocol::newOutputStream(const URL& url, bool append) {
    (void)url;
    (void)append;
    return nullptr;  // data: is read-only
}
