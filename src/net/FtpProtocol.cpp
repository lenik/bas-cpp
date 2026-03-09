#include "FtpProtocol.hpp"

#include "CurlInputStream.hpp"
#include "CurlOutputStream.hpp"
#include "URL.hpp"

#include <memory>

std::unique_ptr<InputStream> FtpProtocol::newInputStream(const URL& url) {
    if (url.getScheme() != "ftp") return nullptr;
    return std::make_unique<CurlInputStream>(url.spec());
}

std::unique_ptr<OutputStream> FtpProtocol::newOutputStream(const URL& url, bool append) {
    if (url.getScheme() != "ftp") return nullptr;
    return std::make_unique<CurlOutputStream>(url.spec(), append);
}
