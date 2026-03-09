#ifndef FTPPROTOCOL_H
#define FTPPROTOCOL_H

#include "NetProtocol.hpp"

class FtpProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "ftp"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // FTPPROTOCOL_H
