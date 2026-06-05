#ifndef BAS_SECURITY_ACCESS_DECISION_RESOLVER_HPP
#define BAS_SECURITY_ACCESS_DECISION_RESOLVER_HPP

#include "AccessDecision.hpp"
#include "Binding.hpp"

#include <vector>

namespace bas::security {

class AccessDecisionResolver {
  public:
    virtual ~AccessDecisionResolver() = default;

    virtual AccessDecision resolve(const Permission& request,
                                   const std::vector<ACEntry>& entries) const = 0;
};

class DefaultAccessDecisionResolver : public AccessDecisionResolver {
  public:
    AccessDecision resolve(const Permission& request,
                           const std::vector<ACEntry>& entries) const override;
};

/** @deprecated Use AccessDecisionResolver. */
class ACResolvePolicy {
  public:
    virtual ~ACResolvePolicy() = default;

    virtual AccessEffect resolve(const std::vector<ACEMatch>& matches) const = 0;
};

class DefaultACResolvePolicy : public ACResolvePolicy {
  public:
    AccessEffect resolve(const std::vector<ACEMatch>& matches) const override;
};

} // namespace bas::security

#endif
