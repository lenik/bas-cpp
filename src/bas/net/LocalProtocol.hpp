#ifndef LOCALPROTOCOL_H
#define LOCALPROTOCOL_H

#include "NetProtocol.hpp"

class LocalProtocol : public NetProtocol {
public:
    std::string getProtocolName() const override { return "file"; }
    std::unique_ptr<InputStream> newInputStream(const URL& url) override;
    std::unique_ptr<RandomInputStream> newRandomInputStream(const URL& url) override;
    std::unique_ptr<OutputStream> newOutputStream(const URL& url, bool append = false) override;
};

#endif // LOCALPROTOCOL_H
