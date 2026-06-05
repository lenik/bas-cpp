#ifndef BAS_SECURITY_IDENTITY_HPP
#define BAS_SECURITY_IDENTITY_HPP

#include "Realm.hpp"
#include "Types.hpp"

#include "../fmt/JsonForm.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

struct IdentityRef {
    std::string type;
    Realm realm;
    std::string name;

    bool operator==(const IdentityRef& other) const {
        if (type != other.type || name != other.name) {
            return false;
        }
        return realm.same(other.realm);
    }

    bool operator!=(const IdentityRef& other) const { return !(*this == other); }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

struct Identity {
    std::string type;
    std::string name;
    Realm realm;

    std::string displayName;
    std::string serviceId;

    IdentitySource source = IdentitySource::Unknown;
    IdentityState state = IdentityState::Unknown;

    std::chrono::system_clock::time_point issuedAt{};
    std::optional<std::chrono::system_clock::time_point> expiresAt;

    JsonObject attributes;

    IdentityRef ref() const { return IdentityRef{type, realm, name}; }

    bool isExpired(std::chrono::system_clock::time_point now) const {
        return expiresAt.has_value() && now >= *expiresAt;
    }

    bool isActive(std::chrono::system_clock::time_point now) const {
        return state.isActive() && !isExpired(now);
    }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

struct IdentitySet {
    std::optional<Identity> primary;
    std::vector<Identity> identities;

    bool empty() const { return !primary.has_value() && identities.empty(); }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

inline IdentityRef roleRef(const Realm& realm, const std::string& roleName) {
    return IdentityRef{"role", realm, roleName};
}

} // namespace bas::security

#endif
