#ifndef BAS_SECURITY_PERMISSION_HPP
#define BAS_SECURITY_PERMISSION_HPP

#include <string>
#include <string_view>
#include <vector>

namespace bas::security {

struct Permission {
    std::string action;
    std::string resource;

    Permission() = default;

    explicit Permission(std::string_view legacyPattern) { *this = parse(legacyPattern); }

    bool operator==(const Permission& other) const {
        return action == other.action && resource == other.resource;
    }

    bool empty() const { return action.empty() && resource.empty(); }

    /** Pattern string used for wildcard matching (action when resource is empty). */
    std::string pattern() const { return action; }

    std::string toString() const {
        if (resource.empty()) {
            return action;
        }
        return action + ':' + resource;
    }

    static Permission parse(std::string_view text) {
        Permission permission;
        if (text.empty()) {
            return permission;
        }
        const auto colon = text.find(':');
        if (colon != std::string_view::npos && colon > 0) {
            permission.action = std::string(text.substr(0, colon));
            permission.resource = std::string(text.substr(colon + 1));
        } else {
            permission.action = std::string(text);
        }
        return permission;
    }
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
