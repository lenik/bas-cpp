#include "access_controller.hpp"

#include "access_denied.hpp"
#include "realm.hpp"

#include <algorithm>

namespace bas::security {

namespace {

void syncCredentialRealmFromIdentities(Credential& cred, const IdentitySet& set) {
    const Identity* source = nullptr;
    if (set.primary.has_value()) {
        source = &*set.primary;
    } else if (!set.identities.empty()) {
        source = &set.identities.front();
    }
    if (!source || source->realm.empty()) {
        return;
    }
    if (cred.meta.realm.empty()) {
        cred.meta.realm = source->realm;
        return;
    }
    if (cred.meta.realm.type.empty() && !source->realm.type.empty()) {
        cred.meta.realm.type = source->realm.type;
    }
    if (cred.meta.realm.name.empty() && !source->realm.name.empty()) {
        cred.meta.realm.name = source->realm.name;
    }
    if (cred.meta.realm.uuid.empty() && !source->realm.uuid.empty()) {
        cred.meta.realm.uuid = source->realm.uuid;
    }
}

} // namespace

AccessController::AccessController(std::shared_ptr<ACList> acl,
                     std::shared_ptr<CredentialManager> credentialManager,
                     std::shared_ptr<IdentityRegistry> identityRegistry,
                     std::shared_ptr<LoginPolicy> loginPolicy,
                     std::shared_ptr<PermissionMatcher> permissionMatcher,
                     std::shared_ptr<ACResolvePolicy> resolvePolicy)
    : m_acl(std::move(acl)), m_credentialManager(std::move(credentialManager)),
      m_identityRegistry(std::move(identityRegistry)), m_loginPolicy(std::move(loginPolicy)),
      m_permissionMatcher(std::move(permissionMatcher)), m_resolvePolicy(std::move(resolvePolicy)) {}

void AccessController::setLoginUi(std::shared_ptr<LoginUi> ui) { m_loginUi = std::move(ui); }

const std::vector<Realm>& AccessController::realms() const { return m_identityRegistry->realms(); }

std::optional<Realm> AccessController::findRealmByName(const std::string& name) const {
    return m_identityRegistry->findRealmByName(name);
}

Realm AccessController::resolveRealmHint(Realm hint) const {
    return m_identityRegistry->resolveRealmHint(hint);
}

void AccessController::start() {
    ACRequestOptions options;
    options.interactive = false;
    options.allowAutoLogin = true;
    options.reason = "AccessController startup";
    tryAutoLogin("", options);
}

std::vector<Identity> AccessController::currentIdentities() const { return m_activeIdentities; }

void AccessController::activate(const IdentitySet& identitySet) {
    auto isActive = [this](const Identity& id) {
        return std::any_of(m_activeIdentities.begin(), m_activeIdentities.end(),
                           [&](const Identity& active) { return active.ref() == id.ref(); });
    };

    auto activateIfNew = [&](const Identity& id) {
        if (!isActive(id)) {
            activateOne(id);
        }
    };

    for (const auto& identity : identitySet.identities) {
        activateIfNew(identity);
    }
    if (identitySet.primary.has_value()) {
        activateIfNew(*identitySet.primary);
    }
}

void AccessController::logout(const IdentityRef& identity) {
    m_suppressAutoLogin = true;
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) { return id.ref() == identity; }),
        m_activeIdentities.end());
}

void AccessController::logoutType(const std::string& identityType) {
    m_suppressAutoLogin = true;
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) { return id.type == identityType; }),
        m_activeIdentities.end());
}

void AccessController::logoutAll() {
    m_suppressAutoLogin = true;
    m_activeIdentities.clear();
}

void AccessController::logoutRealm(const Realm& realm) {
    m_suppressAutoLogin = true;
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) { return realmScopedEqual(id.realm, realm); }),
        m_activeIdentities.end());
}

ACMode AccessController::checkPermission(const Permission& permission) const {
    return normalizeResult(checkPermissionRaw(permission, m_activeIdentities));
}

ACMode AccessController::checkPermission(const Permission& permission,
                                  const ACRequestOptions& options) const {
    if (options.realmHint.empty() || !options.realmHint.hasKey()) {
        return checkPermission(permission);
    }
    std::vector<Identity> filtered;
    filtered.reserve(m_activeIdentities.size());
    for (const auto& id : m_activeIdentities) {
        if (realmMatches(id.realm, options.realmHint)) {
            filtered.push_back(id);
        }
    }
    return normalizeResult(checkPermissionRaw(permission, filtered));
}

ACMode AccessController::requestPermission(const Permission& permission, const ACRequestOptions& options) {
    ACMode mode = checkPermissionRaw(permission);
    if (mode != ACMode::Unknown) {
        return normalizeResult(mode);
    }

    if (options.allowAutoLogin && !m_suppressAutoLogin) {
        tryAutoLogin(permission, options);
        mode = checkPermissionRaw(permission);
        if (mode != ACMode::Unknown) {
            return normalizeResult(mode);
        }
    }

    const auto services = selectServices(permission, options);
    for (auto& service : services) {
        if (!service) {
            continue;
        }
        const bool changed = tryLoginWithService(*service, permission, options);
        if (!changed) {
            continue;
        }
        mode = checkPermissionRaw(permission);
        if (mode != ACMode::Unknown) {
            return normalizeResult(mode);
        }
    }

    return ACMode::Deny;
}

void AccessController::requirePermission(const Permission& permission, const ACRequestOptions& options) {
    if (requestPermission(permission, options) != ACMode::Allow) {
        throw AccessDenied(permission);
    }
}

ACMode AccessController::checkPermissionRaw(const Permission& permission) const {
    return checkPermissionRaw(permission, m_activeIdentities);
}

ACMode AccessController::checkPermissionRaw(const Permission& permission,
                                     const std::vector<Identity>& identities) const {
    return m_acl->checkAny(identities, permission, *m_permissionMatcher, *m_resolvePolicy);
}

ACMode AccessController::normalizeResult(ACMode mode) const {
    if (mode == ACMode::Unknown) {
        return ACMode::Deny;
    }
    return mode;
}

void AccessController::tryAutoLogin(const Permission& permission, const ACRequestOptions& options) {
    const auto services = m_identityRegistry->findAutoLoginServices();
    for (auto& service : services) {
        if (!service) {
            continue;
        }
        LoginRequest request;
        request.identityType = service->identityType();
        request.permission = permission;
        request.realmHint = options.realmHint;
        request.interactive = false;
        request.context = options.context;

        const LoginResult result = service->login(request);
        if (result.status == LoginStatus::Success && result.identities.has_value()) {
            activate(*result.identities);
        }
    }
}

std::vector<std::shared_ptr<IdentityService>>
AccessController::selectServices(const Permission& /*permission*/, const ACRequestOptions& options) const {
    std::vector<std::shared_ptr<IdentityService>> pool;
    if (!options.realmHint.empty()) {
        pool = m_identityRegistry->findByRealm(options.realmHint);
    } else {
        pool = m_identityRegistry->list();
    }
    if (!options.preferredIdentityTypes.empty()) {
        std::vector<std::shared_ptr<IdentityService>> selected;
        for (const auto& type : options.preferredIdentityTypes) {
            for (const auto& service : pool) {
                if (service && service->identityType() == type) {
                    selected.push_back(service);
                }
            }
        }
        return selected;
    }
    return pool;
}

bool AccessController::tryLoginWithService(IdentityService& service, const Permission& permission,
                                    const ACRequestOptions& options) {
    if (options.interactive && service.canAutoLogin()) {
        return false;
    }

    CredentialRequest credentialRequest;
    credentialRequest.types = service.supportedCredentialTypes();
    credentialRequest.identityType = service.identityType();
    credentialRequest.subjectHint = options.nameHint;
    credentialRequest.serviceHint = service.id();
    credentialRequest.realmHint = options.realmHint;
    credentialRequest.permission = permission;
    credentialRequest.interactive = false;
    credentialRequest.reason = options.reason;
    credentialRequest.context = options.context;

    std::optional<Credential> credential = m_credentialManager->get(credentialRequest);

    LoginRequest loginRequest;
    loginRequest.identityType = service.identityType();
    loginRequest.nameHint = options.nameHint;
    loginRequest.realmHint = options.realmHint;
    loginRequest.permission = permission;
    loginRequest.credential = std::move(credential);
    loginRequest.interactive = false;
    loginRequest.context = options.context;

    LoginResult result = service.login(loginRequest);

    if (result.status == LoginStatus::Success && result.identities.has_value()) {
        activate(*result.identities);
        return true;
    }

    if (result.status == LoginStatus::NeedInteraction && options.interactive &&
        result.form.has_value() && m_loginUi) {
        credentialRequest.interactive = true;
        if (auto interactiveCred =
                m_loginUi->requestCredential(*result.form, credentialRequest)) {
            loginRequest.credential = std::move(interactiveCred);
            loginRequest.interactive = true;
            result = service.login(loginRequest);
            if (result.status == LoginStatus::Success && result.identities.has_value()) {
                m_suppressAutoLogin = false;
                activate(*result.identities);
                if (loginRequest.credential.has_value()) {
                    Credential toStore = std::move(*loginRequest.credential);
                    syncCredentialRealmFromIdentities(toStore, *result.identities);
                    m_credentialManager->put(std::move(toStore));
                }
                return true;
            }
        }
    }

    return false;
}

std::size_t AccessController::activateCachedCredentials(const ACRequestOptions& options) {
    std::size_t activated = 0;
    for (const auto& service : m_identityRegistry->list()) {
        if (!service || service->canAutoLogin()) {
            continue;
        }

        CredentialRequest listRequest;
        listRequest.types = service->supportedCredentialTypes();
        listRequest.serviceHint = service->id();
        listRequest.realmHint = options.realmHint;

        for (const auto& info : m_credentialManager->list(listRequest)) {
            auto credential = m_credentialManager->load(info.ref);
            if (!credential.has_value()) {
                continue;
            }

            const bool realmMissing = credential->meta.realm.empty();

            LoginRequest loginRequest;
            loginRequest.identityType = service->identityType();
            loginRequest.nameHint = credential->meta.subjectHint;
            loginRequest.realmHint = credential->meta.realm;
            loginRequest.credential = std::move(credential);
            loginRequest.interactive = false;
            loginRequest.context = options.context;

            const LoginResult result = service->login(loginRequest);
            if (result.status != LoginStatus::Success || !result.identities.has_value()) {
                continue;
            }
            activate(*result.identities);
            if (realmMissing && loginRequest.credential.has_value()) {
                Credential toStore = std::move(*loginRequest.credential);
                syncCredentialRealmFromIdentities(toStore, *result.identities);
                if (!toStore.meta.realm.empty()) {
                    m_credentialManager->put(std::move(toStore));
                }
            }
            ++activated;
        }
    }
    return activated;
}

bool AccessController::login(const ACRequestOptions& options) {
    ACRequestOptions opts = options;
    opts.realmHint = resolveRealmHint(opts.realmHint);
    opts.interactive = true;
    opts.allowAutoLogin = false;
    if (opts.preferredIdentityTypes.empty()) {
        opts.preferredIdentityTypes = {"user"};
    }
    if (opts.reason.empty()) {
        opts.reason = "login";
    }

    if (activateCachedCredentials(opts) > 0) {
        m_suppressAutoLogin = false;
        return true;
    }

    const auto services = selectServices("", opts);
    for (auto& service : services) {
        if (!service || service->canAutoLogin()) {
            continue;
        }
        if (tryLoginWithService(*service, "", opts)) {
            m_suppressAutoLogin = false;
            return true;
        }
    }
    return false;
}

void AccessController::removeConflicting(const Identity& incoming) {
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) {
                           return id.type == incoming.type &&
                                  realmScopedEqual(id.realm, incoming.realm);
                       }),
        m_activeIdentities.end());
}

void AccessController::activateOne(const Identity& incoming) {
    const auto action = m_loginPolicy->resolveConflict(m_activeIdentities, incoming);

    switch (action) {
    case LoginConflictAction::KeepExisting:
        if (m_loginPolicy->canCoexist(m_activeIdentities, incoming)) {
            m_activeIdentities.push_back(incoming);
        }
        break;

    case LoginConflictAction::ReplaceExisting:
        removeConflicting(incoming);
        m_activeIdentities.push_back(incoming);
        break;

    case LoginConflictAction::RejectIncoming:
        break;

    case LoginConflictAction::AskUser:
        // Without GUI provider, treat as reject incoming.
        break;
    }
}

} // namespace bas::security
