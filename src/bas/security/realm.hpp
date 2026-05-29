#ifndef BAS_SECURITY_REALM_HPP
#define BAS_SECURITY_REALM_HPP

#include <string>

namespace bas::security {

struct Realm {
    std::string type; // e.g. global, device, app
    std::string uuid;
    std::string name;
    std::string description;
    std::string image;

    bool empty() const {
        return type.empty() && uuid.empty() && name.empty() && description.empty() && image.empty();
    }

    bool hasKey() const { return !type.empty() || !uuid.empty() || !name.empty(); }

    static const Realm GLOBAL;
};

/** Hint filter: empty hint matches any stored realm. */
bool realmMatches(const Realm& stored, const Realm& hint);

/** Identity / ACL realm equality; empty on either side is a wildcard. */
bool realmSame(const Realm& a, const Realm& b);

/** Strict realm slot match for login concurrency (empty matches empty only). */
bool realmScopedEqual(const Realm& a, const Realm& b);

std::string realmDisplayLabel(const Realm& realm);

/** Stable key for maps / storage indexes. */
std::string realmStorageKey(const Realm& realm);

} // namespace bas::security

#endif
