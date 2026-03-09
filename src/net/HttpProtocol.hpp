#ifndef HTTPPROTOCOL_H
#define HTTPPROTOCOL_H

#include "NetProtocol.hpp"

class HttpProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "http"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // HTTPPROTOCOL_H
