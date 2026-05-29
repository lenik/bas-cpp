#ifndef BAS_SECURITY_PERMISSION_HPP
#define BAS_SECURITY_PERMISSION_HPP

#include "types.hpp"

namespace bas::security {

class PermissionMatcher {
  public:
    virtual ~PermissionMatcher() = default;

    virtual bool matches(const Permission& pattern, const Permission& permission) const = 0;

    virtual int specificity(const Permission& pattern) const = 0;
};

class DefaultPermissionMatcher : public PermissionMatcher {
  public:
    bool matches(const Permission& pattern, const Permission& permission) const override;

    int specificity(const Permission& pattern) const override;
};

std::vector<std::string_view> splitPermissionTokens(std::string_view permission);

} // namespace bas::security

#endif
