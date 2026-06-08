#ifndef BAS_SECURITY_SECURITY_MANAGER_HPP
#define BAS_SECURITY_SECURITY_MANAGER_HPP

#include "AccessDecision.hpp"
#include "AccessDecisionResolver.hpp"
#include "CommandSupport.hpp"
#include "CredentialManager.hpp"
#include "IdentityRegistry.hpp"
#include "LoginUi.hpp"
#include "Permission.hpp"
#include "PolicyStore.hpp"
#include "Realm.hpp"
#include "Subject.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

class SecurityManager : public ICommandSupport {
  public:
    SecurityManager(std::shared_ptr<PolicyStore> policyStore,
                    std::shared_ptr<CredentialManager> credentialManager,
                    std::shared_ptr<IdentityRegistry> identityRegistry,
                    std::shared_ptr<PermissionMatcher> permissionMatcher,
                    std::shared_ptr<ACResolvePolicy> resolvePolicy);

    void setDecisionResolver(std::shared_ptr<AccessDecisionResolver> resolver);

    void setLoginUi(std::shared_ptr<LoginUi> ui);

    const std::vector<Realm>& realms() const;
    std::optional<Realm> findRealmByName(const std::string& name) const;
    Realm resolveRealmHint(Realm hint) const;

    void start();

    std::vector<Identity> currentIdentities() const;

    Subject currentSubject() const;

    void activate(const IdentitySet& identitySet);

    void logout(const IdentityRef& identity);

    void logoutType(const std::string& identityType);

    void logoutAll();

    bool autoLoginSuppressed() const { return m_suppressAutoLogin; }

    AccessDecision evaluate(const Subject& subject, const Permission& permission) const;

    AccessEffect checkPermission(const Permission& permission) const;

    AccessEffect checkPermission(const Permission& permission,
                                 const AccessRequestOptions& options) const;

    /** Verify credentials and return identities without activating a session. */
    std::optional<IdentitySet> authenticate(Credential credential,
                                            const AccessRequestOptions& options = {});

    /** Check permission for an explicit subject without changing the active session. */
    AccessEffect checkSubjectPermission(const Permission& permission, const Subject& subject,
                                        const AccessRequestOptions& options = {}) const;

    AccessEffect requestPermission(const Permission& permission,
                                   const AccessRequestOptions& options = {});

    void requirePermission(const Permission& permission, const AccessRequestOptions& options = {});

    std::size_t activateCachedCredentials(const AccessRequestOptions& options = {});

    void logoutRealm(const Realm& realm);

    bool login(const AccessRequestOptions& options = {});

    void setCommandDefaults(Realm defaultRealm, std::string defaultSubject);

    void setCommandDefaultRealm(Realm realm) { m_cmdDefaultRealm = std::move(realm); }

    Realm commandDefaultRealm() const { return m_cmdDefaultRealm; }

    std::string commandDefaultSubject() const { return m_cmdDefaultSubject; }

    std::shared_ptr<PolicyStore> policyStore() const { return m_policyStore; }

    /** @deprecated Use policyStore(). */
    std::shared_ptr<PolicyStore> acl() const { return m_policyStore; }

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    AccessEffect checkPermissionRaw(const Permission& permission) const;
    AccessEffect checkPermissionRaw(const Permission& permission,
                                    const std::vector<Identity>& identities) const;
    AccessEffect normalizeResult(AccessEffect mode) const;
    void tryAutoLogin(const Permission& permission, const AccessRequestOptions& options);
    std::vector<std::shared_ptr<IdentityService>>
    selectServices(const Permission& permission, const AccessRequestOptions& options) const;
    bool tryLoginWithService(IdentityService& service, const Permission& permission,
                             const AccessRequestOptions& options);
    void clearLoginSession(const Realm& realm);
    void removeIdentityRef(const IdentityRef& ref);

    std::shared_ptr<PolicyStore> m_policyStore;
    std::shared_ptr<CredentialManager> m_credentialManager;
    std::shared_ptr<IdentityRegistry> m_identityRegistry;
    std::shared_ptr<PermissionMatcher> m_permissionMatcher;
    std::shared_ptr<ACResolvePolicy> m_resolvePolicy;
    std::shared_ptr<AccessDecisionResolver> m_decisionResolver;
    std::shared_ptr<LoginUi> m_loginUi;

    std::vector<Identity> m_activeIdentities;
    bool m_suppressAutoLogin = false;

    Realm m_cmdDefaultRealm;
    std::string m_cmdDefaultSubject;
};

} // namespace bas::security

#endif
