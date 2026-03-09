#ifndef DATAPROTOCOL_H
#define DATAPROTOCOL_H

#include "NetProtocol.hpp"

class DataProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "data"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // DATAPROTOCOL_H
