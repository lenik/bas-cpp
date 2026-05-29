#include "json.hpp"

#include "bas/script/boost_json.hpp"
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

std::string acModeToString(ACMode mode) {
    switch (mode) {
    case ACMode::Allow:
        return "allow";
    case ACMode::Deny:
        return "deny";
    default:
        return "unknown";
    }
}

ACMode acModeFromString(std::string_view s) {
    if (s == "allow") {
        return ACMode::Allow;
    }
    if (s == "deny") {
        return ACMode::Deny;
    }
    return ACMode::Unknown;
}

std::string identityStateToString(IdentityState state) {
    switch (state) {
    case IdentityState::Active:
        return "active";
    case IdentityState::Expired:
        return "expired";
    case IdentityState::Revoked:
        return "revoked";
    case IdentityState::LoggedOut:
        return "logged_out";
    default:
        return "unknown";
    }
}

IdentityState identityStateFromString(std::string_view s) {
    if (s == "active") {
        return IdentityState::Active;
    }
    if (s == "expired") {
        return IdentityState::Expired;
    }
    if (s == "revoked") {
        return IdentityState::Revoked;
    }
    if (s == "logged_out") {
        return IdentityState::LoggedOut;
    }
    return IdentityState::Unknown;
}

std::string identitySourceToString(IdentitySource source) {
    switch (source) {
    case IdentitySource::Direct:
        return "direct";
    case IdentitySource::Derived:
        return "derived";
    case IdentitySource::Auto:
        return "auto";
    case IdentitySource::System:
        return "system";
    default:
        return "unknown";
    }
}

IdentitySource identitySourceFromString(std::string_view s) {
    if (s == "direct") {
        return IdentitySource::Direct;
    }
    if (s == "derived") {
        return IdentitySource::Derived;
    }
    if (s == "auto") {
        return IdentitySource::Auto;
    }
    if (s == "system") {
        return IdentitySource::System;
    }
    return IdentitySource::Unknown;
}

void realmJsonIn(Realm& realm, const boost::json::value& in) {
    if (in.is_string()) {
        realm = Realm{};
        realm.name = in.as_string().c_str();
        return;
    }
    if (!in.is_object()) {
        realm = Realm{};
        return;
    }
    const auto& o = in.as_object();
    realm.type = boost::json::get_string(o, "type", "");
    realm.uuid = boost::json::get_string(o, "uuid", "");
    realm.name = boost::json::get_string(o, "name", "");
    realm.description = boost::json::get_string(o, "description", "");
    realm.image = boost::json::get_string(o, "image", "");
}

void realmJsonOut(const Realm& realm, boost::json::object& o) {
    if (realm.empty()) {
        return;
    }
    if (!realm.type.empty()) {
        o["type"] = realm.type;
    }
    if (!realm.uuid.empty()) {
        o["uuid"] = realm.uuid;
    }
    if (!realm.name.empty()) {
        o["name"] = realm.name;
    }
    if (!realm.description.empty()) {
        o["description"] = realm.description;
    }
    if (!realm.image.empty()) {
        o["image"] = realm.image;
    }
}

void identityRefJsonIn(IdentityRef& ref, const boost::json::object& o) {
    ref.type = boost::json::get_string(o, "type", "");
    ref.name = boost::json::get_string(o, "name", "");
    if (auto* realmVal = o.if_contains("realm")) {
        realmJsonIn(ref.realm, *realmVal);
    } else {
        ref.realm = Realm{};
    }
}

void identityRefJsonOut(const IdentityRef& ref, boost::json::object& o) {
    o["type"] = ref.type;
    o["name"] = ref.name;
    if (!ref.realm.empty()) {
        boost::json::object realmObj;
        realmJsonOut(ref.realm, realmObj);
        o["realm"] = realmObj;
    }
}

void acRuleJsonIn(ACRule& rule, const boost::json::object& o) {
    rule.permission = boost::json::get_string(o, "permission", "");
    rule.mode = acModeFromString(boost::json::get_string(o, "mode", ""));
}

void acRuleJsonOut(const ACRule& rule, boost::json::object& o) {
    o["permission"] = rule.permission;
    o["mode"] = acModeToString(rule.mode);
}

void acEntryJsonIn(ACEntry& entry, const boost::json::object& o) {
    if (auto* identity = o.if_contains("identity")) {
        if (identity->is_object()) {
            identityRefJsonIn(entry.identity, identity->as_object());
        }
    }
    if (auto* rule = o.if_contains("rule")) {
        if (rule->is_object()) {
            acRuleJsonIn(entry.rule, rule->as_object());
        }
    }
}

void acEntryJsonOut(const ACEntry& entry, boost::json::object& o) {
    boost::json::object identity;
    identityRefJsonOut(entry.identity, identity);
    boost::json::object rule;
    acRuleJsonOut(entry.rule, rule);
    o["identity"] = identity;
    o["rule"] = rule;
}

void ACListJsonForm::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    m_list.clear();
    if (auto* entries = o.if_contains("entries")) {
        if (!entries->is_array()) {
            return;
        }
        for (auto& item : entries->as_array()) {
            if (!item.is_object()) {
                continue;
            }
            ACEntry entry;
            acEntryJsonIn(entry, item.as_object());
            m_list.add(entry);
        }
    }
}

void ACListJsonForm::jsonOut(boost::json::object& obj, const JsonFormOptions& /*opts*/) {
    boost::json::array entries;
    for (const auto& entry : m_list.entries()) {
        boost::json::object item;
        acEntryJsonOut(entry, item);
        entries.push_back(item);
    }
    obj["entries"] = entries;
}

void identityJsonIn(Identity& identity, boost::json::object& o) {
    identity.type = boost::json::get_string(o, "type", "");
    identity.name = boost::json::get_string(o, "name", "");
    if (auto* realmVal = o.if_contains("realm")) {
        realmJsonIn(identity.realm, *realmVal);
    } else {
        identity.realm = Realm{};
    }
    identity.displayName = boost::json::get_string(o, "displayName", "");
    identity.serviceId = boost::json::get_string(o, "serviceId", "");
    identity.source = identitySourceFromString(boost::json::get_string(o, "source", ""));
    identity.state = identityStateFromString(boost::json::get_string(o, "state", ""));
    if (auto* attrs = o.if_contains("attributes")) {
        if (attrs->is_object()) {
            identity.attributes = attrs->as_object();
        }
    }
    if (auto* issued = o.if_contains("issuedAt")) {
        if (issued->is_int64()) {
            identity.issuedAt = epochToTimePoint(issued->as_int64());
        }
    }
    if (auto* expires = o.if_contains("expiresAt")) {
        if (expires->is_int64()) {
            identity.expiresAt = epochToTimePoint(expires->as_int64());
        }
    }
}

void identityJsonOut(const Identity& identity, boost::json::object& o) {
    o["type"] = identity.type;
    o["name"] = identity.name;
    if (!identity.realm.empty()) {
        boost::json::object realmObj;
        realmJsonOut(identity.realm, realmObj);
        o["realm"] = realmObj;
    }
    o["displayName"] = identity.displayName;
    o["serviceId"] = identity.serviceId;
    o["source"] = identitySourceToString(identity.source);
    o["state"] = identityStateToString(identity.state);
    o["attributes"] = identity.attributes;
    o["issuedAt"] = timePointToEpoch(identity.issuedAt);
    if (identity.expiresAt.has_value()) {
        o["expiresAt"] = timePointToEpoch(*identity.expiresAt);
    }
}

void identitySetJsonIn(IdentitySet& set, boost::json::object& o) {
    set.identities.clear();
    set.primary.reset();
    if (auto* primary = o.if_contains("primary")) {
        if (primary->is_object()) {
            Identity id;
            identityJsonIn(id, primary->as_object());
            set.primary = id;
        }
    }
    if (auto* identities = o.if_contains("identities")) {
        if (!identities->is_array()) {
            return;
        }
        for (auto& item : identities->as_array()) {
            if (!item.is_object()) {
                continue;
            }
            Identity id;
            identityJsonIn(id, item.as_object());
            set.identities.push_back(id);
        }
    }
}

void identitySetJsonOut(const IdentitySet& set, boost::json::object& o) {
    if (set.primary.has_value()) {
        boost::json::object primary;
        identityJsonOut(*set.primary, primary);
        o["primary"] = primary;
    }
    boost::json::array identities;
    for (const auto& id : set.identities) {
        boost::json::object item;
        identityJsonOut(id, item);
        identities.push_back(item);
    }
    o["identities"] = identities;
}

void credentialRefJsonIn(CredentialRef& ref, boost::json::object& o) {
    ref.store = boost::json::get_string(o, "store", "");
    ref.id = boost::json::get_string(o, "id", "");
}

void credentialRefJsonOut(const CredentialRef& ref, boost::json::object& o) {
    o["store"] = ref.store;
    o["id"] = ref.id;
}

void credentialMetaJsonIn(CredentialMeta& meta, boost::json::object& o) {
    meta.type = boost::json::get_string(o, "type", "");
    meta.name = boost::json::get_string(o, "name", "");
    meta.subjectHint = boost::json::get_string(o, "subjectHint", "");
    meta.serviceHint = boost::json::get_string(o, "serviceHint", "");
    if (auto* realmVal = o.if_contains("realm")) {
        realmJsonIn(meta.realm, *realmVal);
    } else {
        meta.realm = Realm{};
    }
    if (auto* expires = o.if_contains("expiresAt")) {
        if (expires->is_int64()) {
            meta.expiresAt = epochToTimePoint(expires->as_int64());
        }
    }
    if (auto* attributes = o.if_contains("attributes")) {
        if (attributes->is_object()) {
            meta.attributes = attributes->as_object();
        }
    }
}

void credentialMetaJsonOut(const CredentialMeta& meta, boost::json::object& o) {
    o["type"] = meta.type;
    o["name"] = meta.name;
    o["subjectHint"] = meta.subjectHint;
    o["serviceHint"] = meta.serviceHint;
    if (!meta.realm.empty()) {
        boost::json::object realmObj;
        realmJsonOut(meta.realm, realmObj);
        o["realm"] = realmObj;
    }
    if (meta.expiresAt.has_value()) {
        o["expiresAt"] = timePointToEpoch(*meta.expiresAt);
    }
    if (!meta.attributes.empty()) {
        o["attributes"] = meta.attributes;
    }
}

void credentialInfoJsonIn(CredentialInfo& info, boost::json::object& o) {
    if (auto* ref = o.if_contains("ref")) {
        if (ref->is_object()) {
            credentialRefJsonIn(info.ref, ref->as_object());
        }
    }
    if (auto* meta = o.if_contains("meta")) {
        if (meta->is_object()) {
            credentialMetaJsonIn(info.meta, meta->as_object());
        }
    }
}

void credentialInfoJsonOut(const CredentialInfo& info, boost::json::object& o) {
    boost::json::object refObj;
    credentialRefJsonOut(info.ref, refObj);
    o["ref"] = refObj;
    boost::json::object metaObj;
    credentialMetaJsonOut(info.meta, metaObj);
    o["meta"] = metaObj;
}

void loginFieldJsonIn(LoginField& field, boost::json::object& o) {
    field.name = boost::json::get_string(o, "name", "");
    field.type = boost::json::get_string(o, "type", "");
    field.label = boost::json::get_string(o, "label", "");
    field.required = boost::json::get_bool(o, "required", false);
    if (auto* options = o.if_contains("options")) {
        if (options->is_object()) {
            field.options = options->as_object();
        }
    }
}

void loginFieldJsonOut(const LoginField& field, boost::json::object& o) {
    o["name"] = field.name;
    o["type"] = field.type;
    o["label"] = field.label;
    o["required"] = field.required;
    o["options"] = field.options;
}

void loginActionJsonIn(LoginAction& action, boost::json::object& o) {
    action.name = boost::json::get_string(o, "name", "");
    action.label = boost::json::get_string(o, "label", "");
    if (auto* options = o.if_contains("options")) {
        if (options->is_object()) {
            action.options = options->as_object();
        }
    }
}

void loginActionJsonOut(const LoginAction& action, boost::json::object& o) {
    o["name"] = action.name;
    o["label"] = action.label;
    o["options"] = action.options;
}

void loginFormSpecJsonIn(LoginFormSpec& form, boost::json::object& o) {
    form.title = boost::json::get_string(o, "title", "");
    form.fields.clear();
    form.actions.clear();
    if (auto* fields = o.if_contains("fields")) {
        if (fields->is_array()) {
            for (auto& item : fields->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                LoginField field;
                loginFieldJsonIn(field, item.as_object());
                form.fields.push_back(field);
            }
        }
    }
    if (auto* actions = o.if_contains("actions")) {
        if (actions->is_array()) {
            for (auto& item : actions->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                LoginAction action;
                loginActionJsonIn(action, item.as_object());
                form.actions.push_back(action);
            }
        }
    }
    if (auto* options = o.if_contains("options")) {
        if (options->is_object()) {
            form.options = options->as_object();
        }
    }
}

void loginFormSpecJsonOut(const LoginFormSpec& form, boost::json::object& o) {
    o["title"] = form.title;
    boost::json::array fields;
    for (const auto& field : form.fields) {
        boost::json::object item;
        loginFieldJsonOut(field, item);
        fields.push_back(item);
    }
    o["fields"] = fields;
    boost::json::array actions;
    for (const auto& action : form.actions) {
        boost::json::object item;
        loginActionJsonOut(action, item);
        actions.push_back(item);
    }
    o["actions"] = actions;
    o["options"] = form.options;
}

} // namespace bas::security
