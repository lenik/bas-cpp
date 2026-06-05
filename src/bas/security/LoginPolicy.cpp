#include "LoginPolicy.hpp"
#include "Realm.hpp"

#include <algorithm>
#include <limits>

namespace bas::security {

namespace {

constexpr std::size_t kUnlimited = std::numeric_limits<std::size_t>::max();

void setDefaultRule(std::unordered_map<std::string, IdentityConcurrencyRule>& rules,
                    std::string type, std::size_t maxActive, LoginConflictAction action) {
    IdentityConcurrencyRule rule;
    rule.identityType = std::move(type);
    rule.maxActive = maxActive;
    rule.conflictAction = action;
    rules[rule.identityType] = rule;
}

} // namespace

LoginPolicy::LoginPolicy() { installDefaults(); }

void LoginPolicy::installDefaults() {
    setDefaultRule(m_rules, "user", 1, LoginConflictAction::ReplaceExisting);
    setDefaultRule(m_rules, "role", kUnlimited, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "group", kUnlimited, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "organization", kUnlimited, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "tenant", 1, LoginConflictAction::ReplaceExisting);
    setDefaultRule(m_rules, "device", 1, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "app", 1, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "service", kUnlimited, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "anonymous", 1, LoginConflictAction::KeepExisting);
    setDefaultRule(m_rules, "public", 1, LoginConflictAction::KeepExisting);
}

void LoginPolicy::setRule(IdentityConcurrencyRule rule) {
    m_rules[rule.identityType] = std::move(rule);
}

IdentityConcurrencyRule LoginPolicy::ruleOf(const std::string& identityType) const {
    const auto it = m_rules.find(identityType);
    if (it != m_rules.end()) {
        return it->second;
    }
    IdentityConcurrencyRule fallback;
    fallback.identityType = identityType;
    fallback.maxActive = 1;
    fallback.conflictAction = LoginConflictAction::ReplaceExisting;
    return fallback;
}

bool LoginPolicy::canCoexist(const std::vector<Identity>& active, const Identity& incoming) const {
    const auto rule = ruleOf(incoming.type);
    if (rule.maxActive == kUnlimited) {
        return true;
    }
    std::size_t occupied = 0;
    for (const auto& id : active) {
        if (id.type == incoming.type && id.realm.scopedEqual(incoming.realm) &&
            id.name != incoming.name) {
            ++occupied;
        }
    }
    return occupied < rule.maxActive;
}

LoginConflictAction LoginPolicy::resolveConflict(const std::vector<Identity>& active,
                                                 const Identity& incoming) const {
    const auto rule = ruleOf(incoming.type);
    if (canCoexist(active, incoming)) {
        return LoginConflictAction::KeepExisting;
    }
    return rule.conflictAction;
}

} // namespace bas::security
