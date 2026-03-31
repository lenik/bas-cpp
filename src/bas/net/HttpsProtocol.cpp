#include "HttpsProtocol.hpp"

#include "CurlInputStream.hpp"
#include "CurlOutputStream.hpp"
#include "URL.hpp"

#include <memory>

std::unique_ptr<InputStream> HttpsProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "https") return nullptr;
    return std::make_unique<CurlInputStream>(url.spec());
}

std::unique_ptr<OutputStream> HttpsProtocol::newOutputStream(const URL& url, bool append) {
    if (url.getScheme() != "https") return nullptr;
    return std::make_unique<CurlOutputStream>(url.spec(), append);
}
