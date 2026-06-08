#include "IdentityService.hpp"
#include "UserStore.hpp"

#include <unordered_set>

namespace bas::security {

namespace {

Identity makeIdentity(std::string type, std::string name, Realm realm, std::string displayName,
                      IdentitySource source, const std::string& serviceId) {
    Identity id;
    id.type = std::move(type);
    id.name = std::move(name);
    id.realm = std::move(realm);
    id.displayName = std::move(displayName);
    id.serviceId = serviceId;
    id.source = source;
    id.state = IdentityState::Active;
    id.issuedAt = std::chrono::system_clock::now();
    return id;
}

LoginResult loginFailed(std::string error) {
    LoginResult result;
    result.status = LoginStatus::Failed;
    result.error = std::move(error);
    return result;
}

} // namespace

std::string AnonymousIdentityService::id() const { return "bas.identity.anonymous"; }

std::string AnonymousIdentityService::identityType() const { return "anonymous"; }

std::vector<std::string> AnonymousIdentityService::supportedCredentialTypes() const { return {}; }

std::vector<std::string> AnonymousIdentityService::supportedRealmTypes() const { return {"global"}; }

bool AnonymousIdentityService::canAutoLogin() const { return true; }

LoginResult AnonymousIdentityService::login(const LoginRequest& request) {
    const auto now = std::chrono::system_clock::now();

    Realm realm = request.realmHint;
    if (realm.type.empty()) {
        realm.type = "global";
    }

    Identity anonymous = makeIdentity("anonymous", "default", realm, "Anonymous", IdentitySource::Auto,
                                      id());
    anonymous.issuedAt = now;

    Identity publicIdentity =
        makeIdentity("public", "default", realm, "Public", IdentitySource::Auto, id());
    publicIdentity.issuedAt = now;

    IdentitySet set;
    set.primary = anonymous;
    set.identities.push_back(publicIdentity);

    LoginResult result;
    result.status = LoginStatus::Success;
    result.identities = set;
    return result;
}

void AnonymousIdentityService::logout(const Identity& /*identity*/) {}

StoreIdentityService::StoreIdentityService(std::shared_ptr<UserStore> userStore)
    : m_userStore(std::move(userStore)) {}

void StoreIdentityService::setUserStore(std::shared_ptr<UserStore> userStore) {
    m_userStore = std::move(userStore);
}

std::vector<std::shared_ptr<UserStore>> StoreIdentityService::getBackedStores() const {
    if (m_userStore) {
        return {m_userStore};
    }
    return {};
}

std::string StoreIdentityService::id() const { return "bas.identity.user.store"; }

std::string StoreIdentityService::identityType() const { return "user"; }

std::vector<std::string> StoreIdentityService::supportedCredentialTypes() const {
    return {"password"};
}

std::vector<std::string> StoreIdentityService::supportedRealmTypes() const {
    return {"global", "device", "app"};
}

LoginResult StoreIdentityService::login(const LoginRequest& request) {
    if (!m_userStore) {
        return loginFailed("user store not configured");
    }

    if (!request.credential.has_value()) {
        LoginFormSpec form;
        form.title = "User Login";
        LoginField realmType;
        realmType.name = "realm_type";
        realmType.type = "text";
        realmType.label = "Realm type";
        realmType.required = false;
        if (!request.realmHint.type.empty()) {
            realmType.options["value"] = request.realmHint.type;
        } else {
            realmType.options["value"] = "device";
        }
        LoginField realmName;
        realmName.name = "realm";
        realmName.type = "text";
        realmName.label = "Realm name";
        realmName.required = false;
        if (!request.realmHint.name.empty()) {
            realmName.options["value"] = request.realmHint.name;
        }
        LoginField realmUuid;
        realmUuid.name = "realm_uuid";
        realmUuid.type = "text";
        realmUuid.label = "Realm UUID";
        realmUuid.required = false;
        if (!request.realmHint.uuid.empty()) {
            realmUuid.options["value"] = request.realmHint.uuid;
        }
        LoginField username;
        username.name = "username";
        username.type = "text";
        username.label = "Username";
        username.required = true;
        LoginField password;
        password.name = "password";
        password.type = "password";
        password.label = "Password";
        password.required = true;
        form.fields.push_back(realmType);
        form.fields.push_back(realmName);
        form.fields.push_back(realmUuid);
        form.fields.push_back(username);
        form.fields.push_back(password);

        LoginResult result;
        result.status = LoginStatus::NeedInteraction;
        result.requiredCredentialTypes = {"password"};
        result.form = form;
        return result;
    }

    const Credential& credential = *request.credential;
    if (credential.meta.type != "password") {
        LoginResult result;
        result.status = LoginStatus::Unsupported;
        result.error = "unsupported credential type";
        return result;
    }

    const std::string username = credential.meta.subjectHint.empty() ? request.nameHint
                                                                     : credential.meta.subjectHint;
    if (username.empty()) {
        return loginFailed("username required");
    }

    if (!credential.secret.has_value()) {
        return loginFailed("password required");
    }
    if (!verifyUserPassword(*m_userStore, username, credential.secret->value())) {
        return loginFailed("invalid username or password");
    }

    const auto record = m_userStore->getUserRecord(username);
    if (!record.has_value() || !record->profile.enabled) {
        return loginFailed("user disabled");
    }

    Realm realm = credential.meta.realm.empty() ? request.realmHint : credential.meta.realm;
    if (realm.type.empty()) {
        realm.type = "device";
    }

    std::string displayName = record->profile.displayName.empty() ? username : record->profile.displayName;
    std::vector<std::string> roleNames = record->roles;
    if (roleNames.empty()) {
        roleNames.push_back("operator");
    }

    Identity user =
        makeIdentity("user", username, realm, displayName, IdentitySource::Login, id());

    IdentitySet set;
    set.primary = user;
    set.identities.push_back(user);

    for (const auto& roleName : roleNames) {
        Identity role =
            makeIdentity("role", roleName, realm, roleName, IdentitySource::Login, id());
        set.identities.push_back(role);
    }

    LoginResult result;
    result.status = LoginStatus::Success;
    result.identities = set;
    return result;
}

void StoreIdentityService::logout(const Identity& /*identity*/) {}

} // namespace bas::security
