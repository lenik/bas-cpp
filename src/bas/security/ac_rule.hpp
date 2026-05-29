#ifndef BAS_SECURITY_AC_RULE_HPP
#define BAS_SECURITY_AC_RULE_HPP

#include "identity.hpp"
#include "types.hpp"

#include <vector>

namespace bas::security {

struct ACRule {
    Permission permission;
    ACMode mode = ACMode::Unknown;

    bool operator==(const ACRule& other) const {
        return permission == other.permission && mode == other.mode;
    }
};

struct ACEntry {
    IdentityRef identity;
    ACRule rule;

    bool operator==(const ACEntry& other) const {
        return identity == other.identity && rule == other.rule;
    }
};

struct ACMatch {
    ACEntry entry;
    int permissionSpecificity = 0;
    int identitySpecificity = 0;
};

class ACResolvePolicy {
  public:
    virtual ~ACResolvePolicy() = default;

    virtual ACMode resolve(const std::vector<ACMatch>& matches) const = 0;
};

class DefaultACResolvePolicy : public ACResolvePolicy {
  public:
    ACMode resolve(const std::vector<ACMatch>& matches) const override;
};

int identitySourcePriority(IdentitySource source);

} // namespace bas::security

#endif
