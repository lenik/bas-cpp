#ifndef BAS_SECURITY_POLICY_HPP
#define BAS_SECURITY_POLICY_HPP

#include "ACList.hpp"
#include "Identity.hpp"

#include "../fmt/JsonForm.hpp"

#include <string>

namespace bas::security {

/** Direct grant: identity bound to an ACE. */
struct AccessGrant : public ACEntry {
    IdentityRef identity;

    bool operator==(const AccessGrant& other) const {
        return identity == other.identity && permission == other.permission &&
               effect == other.effect;
    }

    ACEntry ace() const { return ACEntry{permission, effect}; }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

inline AccessGrant makeAccessGrant(IdentityRef identity, Permission permission,
                                   AccessEffect effect) {
    AccessGrant grant;
    grant.identity = std::move(identity);
    grant.permission = std::move(permission);
    grant.effect = effect;
    return grant;
}

/** Associates a named ACList with an identity. */
struct PolicyBinding {
    std::string aclId;
    IdentityRef identity;

    bool operator==(const PolicyBinding& other) const {
        return aclId == other.aclId && identity == other.identity;
    }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

struct ACEMatch {
    ACEntry entry;
    IdentityRef identity;
    int permissionSpecificity = 0;
    int identitySpecificity = 0;
};

} // namespace bas::security

#endif
