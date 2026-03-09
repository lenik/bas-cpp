#ifndef FTPSPROTOCOL_H
#define FTPSPROTOCOL_H

#include "NetProtocol.hpp"

class FtpsProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "ftps"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // FTPSPROTOCOL_H
