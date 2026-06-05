#ifndef BAS_SECURITY_LOGIN_POLICY_HPP
#define BAS_SECURITY_LOGIN_POLICY_HPP

#include "Identity.hpp"
#include "Types.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace bas::security {

struct IdentityConcurrencyRule {
    std::string identityType;

    std::size_t maxActive = 1;

    LoginConflictAction conflictAction = LoginConflictAction::ReplaceExisting;
};

class LoginPolicy {
  public:
    LoginPolicy();

    void setRule(IdentityConcurrencyRule rule);

    IdentityConcurrencyRule ruleOf(const std::string& identityType) const;

    bool canCoexist(const std::vector<Identity>& active, const Identity& incoming) const;

    LoginConflictAction resolveConflict(const std::vector<Identity>& active,
                                        const Identity& incoming) const;

  private:
    void installDefaults();

    std::unordered_map<std::string, IdentityConcurrencyRule> m_rules;
};

} // namespace bas::security

#endif
