#ifndef BAS_SECURITY_PERMISSION_HPP
#define BAS_SECURITY_PERMISSION_HPP

#include <string>
#include <string_view>
#include <vector>

namespace bas::security {

/** Permission: optional action and resource. Empty field means "all".
 *
 * Canonical text form:
 *   action=<action>;resource=<resource>
 * Segments are `;`-separated. Values may be quoted when they contain
 * special characters, e.g. resource="file;special".
 * Either or both keys may be omitted (defaults to all).
 */
struct Permission {
    std::string action;
    std::string resource;

    Permission() = default;

    explicit Permission(std::string_view text) { *this = parse(text); }

    bool operator==(const Permission& other) const {
        return action == other.action && resource == other.resource;
    }

    /** True when both fields are empty (matches all). */
    bool empty() const { return action.empty() && resource.empty(); }

    /** True when action is unrestricted (empty = all). */
    bool actionIsAll() const { return action.empty(); }

    /** True when resource is unrestricted (empty = all). */
    bool resourceIsAll() const { return resource.empty(); }

    std::string toString() const;

    static Permission parse(std::string_view text);
};

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
