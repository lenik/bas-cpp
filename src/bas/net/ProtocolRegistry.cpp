#include "ProtocolRegistry.hpp"

#include "DataProtocol.hpp"
#include "FtpProtocol.hpp"
#include "FtpsProtocol.hpp"
#include "HttpProtocol.hpp"
#include "HttpsProtocol.hpp"
#include "LocalProtocol.hpp"

ProtocolRegistry::ProtocolRegistry() {
    registerProtocol("file", std::make_unique<LocalProtocol>());
    registerProtocol("data", std::make_unique<DataProtocol>());
    registerProtocol("http", std::make_unique<HttpProtocol>());
    registerProtocol("https", std::make_unique<HttpsProtocol>());
    registerProtocol("ftp", std::make_unique<FtpProtocol>());
    registerProtocol("ftps", std::make_unique<FtpsProtocol>());
}

ProtocolRegistry& ProtocolRegistry::instance() {
    static ProtocolRegistry reg;
    return reg;
}

void ProtocolRegistry::registerProtocol(std::string scheme, std::unique_ptr<NetProtocol> protocol) {
    if (protocol) m_protocols[std::move(scheme)] = std::move(protocol);
}

NetProtocol* ProtocolRegistry::getProtocol(const std::string& scheme) const {
    auto it = m_protocols.find(scheme);
    return it != m_protocols.end() ? it->second.get() : nullptr;
}

NetProtocol* ProtocolRegistry::getProtocolFor(const URL& url) const {
    return getProtocol(url.getScheme());
}
