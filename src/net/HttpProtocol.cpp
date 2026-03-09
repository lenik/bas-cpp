#include "HttpProtocol.hpp"

#include "CurlInputStream.hpp"
#include "CurlOutputStream.hpp"
#include "URL.hpp"

#include <memory>

std::unique_ptr<InputStream> HttpProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "http") return nullptr;
    return std::make_unique<CurlInputStream>(url.spec());
}

std::unique_ptr<OutputStream> HttpProtocol::newOutputStream(const URL& url, bool append) {
    if (url.getScheme() != "http") return nullptr;
    return std::make_unique<CurlOutputStream>(url.spec(), append);
}
