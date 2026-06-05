#ifndef BAS_SECURITY_REALM_HPP
#define BAS_SECURITY_REALM_HPP

#include "../fmt/JsonForm.hpp"

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

    bool matchType(const Realm& hint) const;

    /** Hint filter: empty hint matches any stored realm. */
    bool match(const Realm& hint) const;

    /** Identity / ACL realm equality; empty on either side is a wildcard. */
    bool same(const Realm& other) const;

    /** Strict realm slot match for login concurrency (empty matches empty only). */
    bool scopedEqual(const Realm& other) const;

    std::string displayLabel() const;

    /** Stable key for maps / storage indexes. */
    std::string storageKey() const;

    bool operator==(const Realm& other) const;
    bool operator!=(const Realm& other) const;
    
    std::string str() const;
    inline operator std::string() const { return str(); }

    static const Realm GLOBAL;

    void jsonIn(const boost::json::value& in, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

} // namespace bas::security

#endif
