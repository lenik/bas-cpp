#include "SecurityManager.hpp"

#include "AccessDenied.hpp"
#include "Identity.hpp"
#include "PolicyStore.hpp"
#include "Realm.hpp"

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

std::vector<Identity> filterIdentitiesByRealm(const std::vector<Identity>& identities,
                                              const Realm& realmHint) {
    if (!realmHint.hasKey()) {
        return identities;
    }
    std::vector<Identity> filtered;
    filtered.reserve(identities.size());
    for (const auto& id : identities) {
        if (id.realm.match(realmHint)) {
            filtered.push_back(id);
        }
    }
    return filtered;
}

} // namespace

SecurityManager::SecurityManager(std::shared_ptr<PolicyStore> policyStore,
                                 std::shared_ptr<CredentialManager> credentialManager,
                                 std::shared_ptr<IdentityRegistry> identityRegistry,
                                 std::shared_ptr<PermissionMatcher> permissionMatcher,
                                 std::shared_ptr<ACResolvePolicy> resolvePolicy)
    : m_policyStore(std::move(policyStore)), m_credentialManager(std::move(credentialManager)),
      m_identityRegistry(std::move(identityRegistry)),
      m_permissionMatcher(std::move(permissionMatcher)), m_resolvePolicy(std::move(resolvePolicy)),
      m_decisionResolver(std::make_shared<DefaultAccessDecisionResolver>()) {}

void SecurityManager::setDecisionResolver(std::shared_ptr<AccessDecisionResolver> resolver) {
    m_decisionResolver = std::move(resolver);
}

void SecurityManager::setLoginUi(std::shared_ptr<LoginUi> ui) { m_loginUi = std::move(ui); }

const std::vector<Realm>& SecurityManager::realms() const { return m_identityRegistry->realms(); }

std::optional<Realm> SecurityManager::findRealmByName(const std::string& name) const {
    return m_identityRegistry->findRealmByName(name);
}

Realm SecurityManager::resolveRealmHint(Realm hint) const {
    return m_identityRegistry->resolveRealmHint(hint);
}

void SecurityManager::start() {
    AccessRequestOptions options;
    options.allowConsoleInteraction = false;
    options.allowGuiInteraction = false;
    options.allowAutoLogin = true;
    options.reason = "SecurityManager startup";
    tryAutoLogin(Permission{}, options);
}

std::vector<Identity> SecurityManager::currentIdentities() const { return m_activeIdentities; }

Subject SecurityManager::currentSubject() const {
    return Subject::fromIdentities(m_activeIdentities);
}

AccessDecision SecurityManager::evaluate(const Subject& subject,
                                         const Permission& permission) const {
    std::vector<ACEntry> entries;
    for (const auto& identity : subject.identities) {
        const auto effective = m_policyStore->effectiveEntriesOf(identity.ref());
        entries.insert(entries.end(), effective.begin(), effective.end());
    }
    return m_decisionResolver->resolve(permission, entries);
}

void SecurityManager::activate(const IdentitySet& identitySet) {
    std::vector<Realm> realmsToClear;
    auto considerRealm = [&](const Identity& id) {
        if (!isLoginSessionIdentity(id) || !id.realm.hasKey()) {
            return;
        }
        if (std::none_of(realmsToClear.begin(), realmsToClear.end(),
                         [&](const Realm& realm) { return realm.scopedEqual(id.realm); })) {
            realmsToClear.push_back(id.realm);
        }
    };

    for (const auto& identity : identitySet.identities) {
        considerRealm(identity);
    }
    if (identitySet.primary.has_value()) {
        considerRealm(*identitySet.primary);
    }
    for (const Realm& realm : realmsToClear) {
        clearLoginSession(realm);
    }

    std::vector<Identity> toAdd;
    if (identitySet.primary.has_value()) {
        toAdd.push_back(*identitySet.primary);
    }
    for (const auto& identity : identitySet.identities) {
        if (identitySet.primary.has_value() && identity.ref() == identitySet.primary->ref()) {
            continue;
        }
        toAdd.push_back(identity);
    }
    for (const Identity& id : toAdd) {
        removeIdentityRef(id.ref());
        m_activeIdentities.push_back(id);
    }
}

void SecurityManager::logout(const IdentityRef& identity) {
    m_suppressAutoLogin = true;
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) { return id.ref() == identity; }),
        m_activeIdentities.end());
}

void SecurityManager::logoutType(const std::string& identityType) {
    m_suppressAutoLogin = true;
    m_activeIdentities.erase(
        std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                       [&](const Identity& id) { return id.type == identityType; }),
        m_activeIdentities.end());
}

void SecurityManager::logoutAll() {
    m_suppressAutoLogin = true;
    m_activeIdentities.clear();
}

void SecurityManager::logoutRealm(const Realm& realm) {
    m_suppressAutoLogin = true;
    clearLoginSession(realm);
}

AccessEffect SecurityManager::checkPermission(const Permission& permission) const {
    return normalizeResult(checkPermissionRaw(permission, m_activeIdentities));
}

AccessEffect SecurityManager::checkPermission(const Permission& permission,
                                              const AccessRequestOptions& options) const {
    if (options.realmHint.empty() || !options.realmHint.hasKey()) {
        return checkPermission(permission);
    }
    std::vector<Identity> filtered;
    filtered.reserve(m_activeIdentities.size());
    for (const auto& id : m_activeIdentities) {
        if (id.realm.match(options.realmHint)) {
            filtered.push_back(id);
        }
    }
    return normalizeResult(checkPermissionRaw(permission, filtered));
}

std::optional<IdentitySet> SecurityManager::authenticate(Credential credential,
                                                         const AccessRequestOptions& options) {
    AccessRequestOptions opts = options;
    opts.realmHint = resolveRealmHint(opts.realmHint);

    const auto services = selectServices(Permission{}, opts);
    for (auto& service : services) {
        if (!service || service->canAutoLogin()) {
            continue;
        }

        LoginRequest loginRequest;
        loginRequest.identityType = service->identityType();
        loginRequest.nameHint =
            credential.meta.subjectHint.empty() ? opts.nameHint : credential.meta.subjectHint;
        loginRequest.realmHint =
            credential.meta.realm.empty() ? opts.realmHint : credential.meta.realm;
        loginRequest.credential = std::move(credential);
        loginRequest.interactive = false;
        loginRequest.context = opts.context;

        const LoginResult result = service->login(loginRequest);
        if (result.status == LoginStatus::Success && result.identities.has_value()) {
            return *result.identities;
        }
    }
    return std::nullopt;
}

AccessEffect SecurityManager::checkSubjectPermission(const Permission& permission,
                                                     const Subject& subject,
                                                     const AccessRequestOptions& options) const {
    std::vector<Identity> identities = subject.identities;
    if (options.realmHint.hasKey()) {
        identities = filterIdentitiesByRealm(identities, options.realmHint);
    }
    return normalizeResult(checkPermissionRaw(permission, identities));
}

AccessEffect SecurityManager::requestPermission(const Permission& permission,
                                                const AccessRequestOptions& options) {
    const Realm realmHint =
        options.realmHint.hasKey() ? resolveRealmHint(options.realmHint) : Realm{};
    const auto activeScope = realmHint.hasKey()
                                 ? filterIdentitiesByRealm(m_activeIdentities, realmHint)
                                 : m_activeIdentities;

    AccessEffect mode = checkPermissionRaw(permission, activeScope);
    if (mode != AccessEffect::Unknown) {
        return normalizeResult(mode);
    }

    if (options.allowAutoLogin && !m_suppressAutoLogin) {
        tryAutoLogin(permission, options);
        mode = checkPermissionRaw(permission, activeScope);
        if (mode != AccessEffect::Unknown) {
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
        const auto afterLogin = realmHint.hasKey()
                                    ? filterIdentitiesByRealm(m_activeIdentities, realmHint)
                                    : m_activeIdentities;
        mode = checkPermissionRaw(permission, afterLogin);
        if (mode != AccessEffect::Unknown) {
            return normalizeResult(mode);
        }
    }

    return AccessEffect::Deny;
}

void SecurityManager::requirePermission(const Permission& permission,
                                        const AccessRequestOptions& options) {
    if (requestPermission(permission, options) != AccessEffect::Allow) {
        throw AccessDenied(permission);
    }
}

AccessEffect SecurityManager::checkPermissionRaw(const Permission& permission) const {
    return checkPermissionRaw(permission, m_activeIdentities);
}

AccessEffect SecurityManager::checkPermissionRaw(const Permission& permission,
                                                 const std::vector<Identity>& identities) const {
    return policyCheckAny(*m_policyStore, identities, permission, *m_permissionMatcher,
                          *m_resolvePolicy);
}

AccessEffect SecurityManager::normalizeResult(AccessEffect mode) const {
    if (mode.isUnknown()) {
        return AccessEffect::Deny;
    }
    return mode;
}

void SecurityManager::tryAutoLogin(const Permission& permission,
                                   const AccessRequestOptions& options) {
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
SecurityManager::selectServices(const Permission& /*permission*/,
                                const AccessRequestOptions& options) const {
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

bool SecurityManager::tryLoginWithService(IdentityService& service, const Permission& permission,
                                          const AccessRequestOptions& options) {
    const bool interactive = options.allowGuiInteraction || options.allowConsoleInteraction;
    if (interactive && service.canAutoLogin()) {
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

    if (result.status == LoginStatus::NeedInteraction && interactive && result.form.has_value() &&
        m_loginUi) {
        credentialRequest.interactive = true;
        if (auto interactiveCred = m_loginUi->requestCredential(*result.form, credentialRequest)) {
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

std::size_t SecurityManager::activateCachedCredentials(const AccessRequestOptions& options) {
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

bool SecurityManager::login(const AccessRequestOptions& options) {
    AccessRequestOptions opts = options;
    opts.realmHint = resolveRealmHint(opts.realmHint);
    opts.allowGuiInteraction = true;
    opts.allowConsoleInteraction = true;
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

    const auto services = selectServices(Permission{}, opts);
    for (auto& service : services) {
        if (!service || service->canAutoLogin()) {
            continue;
        }
        if (tryLoginWithService(*service, Permission{}, opts)) {
            m_suppressAutoLogin = false;
            return true;
        }
    }
    return false;
}

void SecurityManager::clearLoginSession(const Realm& realm) {
    m_activeIdentities.erase(std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                                            [&](const Identity& id) {
                                                return isLoginSessionIdentity(id) &&
                                                       id.realm.scopedEqual(realm);
                                            }),
                             m_activeIdentities.end());
}

void SecurityManager::removeIdentityRef(const IdentityRef& ref) {
    m_activeIdentities.erase(std::remove_if(m_activeIdentities.begin(), m_activeIdentities.end(),
                                            [&](const Identity& id) { return id.ref() == ref; }),
                             m_activeIdentities.end());
}

} // namespace bas::security
