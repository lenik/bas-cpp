#include "AccessDecisionResolver.hpp"

#include "Permission.hpp"

#include <algorithm>
#include <limits>
#include <optional>

namespace bas::security {

AccessDecision DefaultAccessDecisionResolver::resolve(
    const Permission& request, const std::vector<ACEntry>& entries) const {
    AccessDecision decision;
    decision.permission = request;
    decision.type = AccessDecisionType::Deny;

    DefaultPermissionMatcher matcher;
    struct ScoredEntry {
        const ACEntry* entry;
        int specificity;
    };
    std::vector<ScoredEntry> matched;
    for (const auto& entry : entries) {
        if (!matcher.matches(entry.permission, request)) {
            continue;
        }
        matched.push_back(ScoredEntry{&entry, matcher.specificity(entry.permission)});
    }
    if (matched.empty()) {
        decision.reason = "no matching ACE";
        return decision;
    }

    int maxSpec = std::numeric_limits<int>::min();
    for (const auto& item : matched) {
        maxSpec = std::max(maxSpec, item.specificity);
    }

    const ACEntry* allowEntry = nullptr;
    bool hasDeny = false;
    for (const auto& item : matched) {
        if (item.specificity != maxSpec) {
            continue;
        }
        if (item.entry->effect.isDeny()) {
            hasDeny = true;
        } else if (item.entry->effect.isAllow() && allowEntry == nullptr) {
            allowEntry = item.entry;
        }
    }

    if (hasDeny) {
        decision.type = AccessDecisionType::Deny;
        decision.reason = "explicit deny";
        return decision;
    }
    if (allowEntry != nullptr) {
        decision.type = AccessDecisionType::Allow;
        decision.reason = "explicit allow";
        return decision;
    }

    decision.reason = "no decisive ACE";
    return decision;
}

AccessEffect DefaultACResolvePolicy::resolve(const std::vector<ACEMatch>& matches) const {
    if (matches.empty()) {
        return AccessEffect::Unknown;
    }

    int maxPermSpec = std::numeric_limits<int>::min();

    for (const auto& match : matches) {
        maxPermSpec = std::max(maxPermSpec, match.permissionSpecificity);
    }

    std::vector<const ACEMatch*> top;

    for (const auto& match : matches) {
        if (match.permissionSpecificity == maxPermSpec) {
            top.push_back(&match);
        }
    }

    for (const ACEMatch* match : top) {
        if (match->entry.effect.isDeny()) {
            return AccessEffect::Deny;
        }
    }

    const ACEMatch* bestAllow = nullptr;

    for (const ACEMatch* match : top) {
        if (!match->entry.effect.isAllow()) {
            continue;
        }

        if (bestAllow == nullptr || match->identitySpecificity > bestAllow->identitySpecificity) {
            bestAllow = match;
        }
    }

    if (bestAllow != nullptr) {
        return AccessEffect::Allow;
    }

    return AccessEffect::Unknown;
}

} // namespace bas::security
