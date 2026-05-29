#include "ac_rule.hpp"

#include <algorithm>
#include <optional>

namespace bas::security {

int identitySourcePriority(IdentitySource source) {
    switch (source) {
    case IdentitySource::Direct:
        return 4;
    case IdentitySource::Derived:
        return 3;
    case IdentitySource::Auto:
        return 2;
    case IdentitySource::System:
        return 1;
    default:
        return 0;
    }
}

ACMode DefaultACResolvePolicy::resolve(const std::vector<ACMatch>& matches) const {
    if (matches.empty()) {
        return ACMode::Unknown;
    }

    int maxPermSpec = std::numeric_limits<int>::min();

    for (const auto& match : matches) {
        maxPermSpec = std::max(maxPermSpec, match.permissionSpecificity);
    }

    std::vector<const ACMatch*> top;

    for (const auto& match : matches) {
        if (match.permissionSpecificity == maxPermSpec) {
            top.push_back(&match);
        }
    }

    for (const ACMatch* match : top) {
        if (match->entry.rule.mode == ACMode::Deny) {
            return ACMode::Deny;
        }
    }

    const ACMatch* bestAllow = nullptr;

    for (const ACMatch* match : top) {
        if (match->entry.rule.mode != ACMode::Allow) {
            continue;
        }

        if (bestAllow == nullptr || match->identitySpecificity > bestAllow->identitySpecificity) {
            bestAllow = match;
        }
    }

    if (bestAllow != nullptr) {
        return ACMode::Allow;
    }

    return ACMode::Unknown;
}

} // namespace bas::security
