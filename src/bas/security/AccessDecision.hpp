#ifndef BAS_SECURITY_ACCESS_DECISION_HPP
#define BAS_SECURITY_ACCESS_DECISION_HPP

#include "ACList.hpp"
#include "Binding.hpp"

#include <string>
#include <vector>

namespace bas::security {

enum class AccessDecisionType {
    Allow,
    Deny,
    ChallengeRequired,
    Error,
};

struct AccessDecision {
    AccessDecisionType type = AccessDecisionType::Deny;

    Permission permission;

    std::string reason;

    std::vector<IdentityRef> matchedIdentities;
    std::vector<AccessGrant> matchedGrants;

    bool allowed() const { return type == AccessDecisionType::Allow; }

    AccessEffect effect() const {
        switch (type) {
        case AccessDecisionType::Allow:
            return AccessEffect::Allow;
        case AccessDecisionType::Deny:
        case AccessDecisionType::ChallengeRequired:
        case AccessDecisionType::Error:
        default:
            return AccessEffect::Deny;
        }
    }
};

/** Options for SecurityManager::requestPermission (formerly ACRequestOptions). */
struct AccessRequestOptions {
    bool allowGuiInteraction = true;
    bool allowConsoleInteraction = true;
    bool allowAutoLogin = true;

    std::vector<std::string> preferredRealms;
    std::vector<std::string> preferredCredentialTypes;
    std::vector<std::string> preferredIdentityTypes;

    std::string nameHint;
    Realm realmHint;
    std::string reason;

    JsonObject context;
};

} // namespace bas::security

#endif
