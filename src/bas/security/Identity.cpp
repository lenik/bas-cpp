#include "Identity.hpp"

#include "bas/script/json.hpp"

#include <boost/json.hpp>

namespace bas::security {

namespace {

int64_t timePointToEpoch(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point epochToTimePoint(int64_t epoch) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{epoch}};
}

} // namespace

void IdentityRef::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    type = boost::json::get_string(o, "type", "");
    name = boost::json::get_string(o, "name", "");
    if (auto* realmVal = o.if_contains("realm")) {
        realm.jsonIn(*realmVal);
    } else {
        realm = Realm{};
    }
}

void IdentityRef::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) const {
    o["type"] = type;
    o["name"] = name;
    if (!realm.empty()) {
        boost::json::object realmObj;
        realm.jsonOut(realmObj);
        o["realm"] = realmObj;
    }
}

void Identity::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    type = boost::json::get_string(o, "type", "");
    name = boost::json::get_string(o, "name", "");
    if (auto* realmVal = o.if_contains("realm")) {
        realm.jsonIn(*realmVal);
    } else {
        realm = Realm{};
    }
    displayName = boost::json::get_string(o, "displayName", "");
    serviceId = boost::json::get_string(o, "serviceId", "");
    source = IdentitySource::fromString(boost::json::get_string(o, "source", ""));
    state = IdentityState::fromString(boost::json::get_string(o, "state", ""));
    if (auto* attrs = o.if_contains("attributes")) {
        if (attrs->is_object()) {
            attributes = attrs->as_object();
        }
    }
    if (auto* issued = o.if_contains("issuedAt")) {
        if (issued->is_int64()) {
            issuedAt = epochToTimePoint(issued->as_int64());
        }
    }
    if (auto* expires = o.if_contains("expiresAt")) {
        if (expires->is_int64()) {
            expiresAt = epochToTimePoint(expires->as_int64());
        }
    }
}

void Identity::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) const {
    o["type"] = type;
    o["name"] = name;
    if (!realm.empty()) {
        boost::json::object realmObj;
        realm.jsonOut(realmObj);
        o["realm"] = realmObj;
    }
    o["displayName"] = displayName;
    o["serviceId"] = serviceId;
    o["source"] = source.str();
    o["state"] = state.str();
    o["attributes"] = attributes;
    o["issuedAt"] = timePointToEpoch(issuedAt);
    if (expiresAt.has_value()) {
        o["expiresAt"] = timePointToEpoch(*expiresAt);
    }
}

void IdentitySet::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    identities.clear();
    primary.reset();
    if (auto* primaryVal = o.if_contains("primary")) {
        if (primaryVal->is_object()) {
            Identity id;
            id.jsonIn(primaryVal->as_object(), opts);
            primary = id;
        }
    }
    if (auto* ids = o.if_contains("identities")) {
        if (!ids->is_array()) {
            return;
        }
        for (auto& item : ids->as_array()) {
            if (!item.is_object()) {
                continue;
            }
            Identity id;
            id.jsonIn(item.as_object(), opts);
            identities.push_back(id);
        }
    }
}

void IdentitySet::jsonOut(boost::json::object& o, const JsonFormOptions& opts) const {
    if (primary.has_value()) {
        boost::json::object primaryObj;
        primary->jsonOut(primaryObj, opts);
        o["primary"] = primaryObj;
    }
    boost::json::array ids;
    for (const auto& id : identities) {
        boost::json::object item;
        id.jsonOut(item, opts);
        ids.push_back(item);
    }
    o["identities"] = ids;
}

} // namespace bas::security
