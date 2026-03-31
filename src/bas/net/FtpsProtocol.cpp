#include "FtpsProtocol.hpp"

#include "CurlInputStream.hpp"
#include "CurlOutputStream.hpp"
#include "URL.hpp"

#include <memory>

std::unique_ptr<InputStream> FtpsProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "ftps") return nullptr;
    return std::make_unique<CurlInputStream>(url.spec());
}

std::unique_ptr<OutputStream> FtpsProtocol::newOutputStream(const URL& url, bool append) {
    if (url.getScheme() != "ftps") return nullptr;
    return std::make_unique<CurlOutputStream>(url.spec(), append);
}
