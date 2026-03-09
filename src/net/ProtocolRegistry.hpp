#ifndef PROTOCOLREGISTRY_H
#define PROTOCOLREGISTRY_H

#include "NetProtocol.hpp"
#include "URL.hpp"

#include <memory>
#include <unordered_map>

/** Registry of scheme -> NetProtocol for URLInputSource/URLOutputTarget. */
class ProtocolRegistry {
public:
    static ProtocolRegistry& instance();

    void registerProtocol(std::string scheme, std::unique_ptr<NetProtocol> protocol);
    NetProtocol* getProtocol(const std::string& scheme) const;

    /** Get protocol for the given URL (by scheme). Returns null if unknown. */
    NetProtocol* getProtocolFor(const URL& url) const;

private:
    ProtocolRegistry();
    std::unordered_map<std::string, std::unique_ptr<NetProtocol>> m_protocols;
};

#endif // PROTOCOLREGISTRY_H
