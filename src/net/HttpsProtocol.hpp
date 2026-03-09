#ifndef HTTPSPROTOCOL_H
#define HTTPSPROTOCOL_H

#include "NetProtocol.hpp"

class HttpsProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "https"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // HTTPSPROTOCOL_H
