#ifndef BAS_SECURITY_ACCESS_CONTROLLER_HPP
#define BAS_SECURITY_ACCESS_CONTROLLER_HPP

#include "ac_list.hpp"
#include "command_support.hpp"
#include "credential.hpp"
#include "identity_registry.hpp"
#include "login_policy.hpp"
#include "login_ui.hpp"
#include "permission.hpp"
#include "realm.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

struct ACRequestOptions {
    bool interactive = true;
    bool allowAutoLogin = true;

    std::vector<std::string> preferredIdentityTypes;

    std::string nameHint;
    Realm realmHint;
    std::string reason;

    JsonObject context;
};

class AccessController : public ICommandSupport {
  public:
    AccessController(std::shared_ptr<ACList> acl,
                     std::shared_ptr<CredentialManager> credentialManager,
                     std::shared_ptr<IdentityRegistry> identityRegistry,
                     std::shared_ptr<LoginPolicy> loginPolicy,
                     std::shared_ptr<PermissionMatcher> permissionMatcher,
                     std::shared_ptr<ACResolvePolicy> resolvePolicy);

    void setLoginUi(std::shared_ptr<LoginUi> ui);

    const std::vector<Realm>& realms() const;
    std::optional<Realm> findRealmByName(const std::string& name) const;
    Realm resolveRealmHint(Realm hint) const;

    void start();

    std::vector<Identity> currentIdentities() const;

    void activate(const IdentitySet& identitySet);

    void logout(const IdentityRef& identity);

    void logoutType(const std::string& identityType);

    void logoutAll();

    bool autoLoginSuppressed() const { return m_suppressAutoLogin; }

    ACMode checkPermission(const Permission& permission) const;

    ACMode checkPermission(const Permission& permission, const ACRequestOptions& options) const;

    ACMode requestPermission(const Permission& permission, const ACRequestOptions& options = {});

    void requirePermission(const Permission& permission, const ACRequestOptions& options = {});

    std::size_t activateCachedCredentials(const ACRequestOptions& options = {});

    void logoutRealm(const Realm& realm);

    bool login(const ACRequestOptions& options = {});

    void setCommandDefaults(Realm defaultRealm, std::string defaultSubject);

    void setCommandDefaultRealm(Realm realm) { m_cmdDefaultRealm = std::move(realm); }

    Realm commandDefaultRealm() const { return m_cmdDefaultRealm; }

    std::string commandDefaultSubject() const { return m_cmdDefaultSubject; }

    std::shared_ptr<ACList> acl() const { return m_acl; }

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    ACMode checkPermissionRaw(const Permission& permission) const;
    ACMode checkPermissionRaw(const Permission& permission,
                              const std::vector<Identity>& identities) const;
    ACMode normalizeResult(ACMode mode) const;
    void tryAutoLogin(const Permission& permission, const ACRequestOptions& options);
    std::vector<std::shared_ptr<IdentityService>> selectServices(
        const Permission& permission, const ACRequestOptions& options) const;
    bool tryLoginWithService(IdentityService& service, const Permission& permission,
                             const ACRequestOptions& options);
    void activateOne(const Identity& incoming);
    void removeConflicting(const Identity& incoming);

    std::shared_ptr<ACList> m_acl;
    std::shared_ptr<CredentialManager> m_credentialManager;
    std::shared_ptr<IdentityRegistry> m_identityRegistry;
    std::shared_ptr<LoginPolicy> m_loginPolicy;
    std::shared_ptr<PermissionMatcher> m_permissionMatcher;
    std::shared_ptr<ACResolvePolicy> m_resolvePolicy;
    std::shared_ptr<LoginUi> m_loginUi;

    std::vector<Identity> m_activeIdentities;
    bool m_suppressAutoLogin = false;

    Realm m_cmdDefaultRealm;
    std::string m_cmdDefaultSubject;
};

} // namespace bas::security

#endif
