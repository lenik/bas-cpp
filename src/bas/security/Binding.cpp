#include "Binding.hpp"

#include "bas/script/json.hpp"

namespace bas::security {

void AccessGrant::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    if (auto* identityVal = o.if_contains("identity")) {
        if (identityVal->is_object()) {
            identity.jsonIn(identityVal->as_object(), opts);
        }
    }
    if (auto* rule = o.if_contains("rule")) {
        if (rule->is_object()) {
            ACEntry ace;
            ace.jsonIn(rule->as_object(), opts);
            permission = ace.permission;
            effect = ace.effect;
            return;
        }
    }
    permission = Permission::parse(boost::json::get_string(o, "permission", ""));
    effect = AccessEffect::fromString(boost::json::get_string(o, "mode", "unknown"));
}

void AccessGrant::jsonOut(boost::json::object& o, const JsonFormOptions& opts) const {
    boost::json::object identityObj;
    identity.jsonOut(identityObj, opts);
    o["identity"] = identityObj;
    o["permission"] = permission.toString();
    o["mode"] = effect.str();
}

void PolicyBinding::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    aclId = boost::json::get_string(o, "aclId", "");
    if (aclId.empty()) {
        aclId = boost::json::get_string(o, "policyId", "");
    }
    if (auto* identityVal = o.if_contains("identity")) {
        if (identityVal->is_object()) {
            identity.jsonIn(identityVal->as_object(), opts);
        }
    }
}

void PolicyBinding::jsonOut(boost::json::object& o, const JsonFormOptions& opts) const {
    o["aclId"] = aclId;
    boost::json::object identityObj;
    identity.jsonOut(identityObj, opts);
    o["identity"] = identityObj;
}

} // namespace bas::security
