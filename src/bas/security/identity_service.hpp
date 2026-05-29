#ifndef BAS_SECURITY_IDENTITY_SERVICE_HPP
#define BAS_SECURITY_IDENTITY_SERVICE_HPP

#include "command_support.hpp"
#include "credential.hpp"
#include "identity.hpp"
#include "realm.hpp"
#include "types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace bas::security {

class UserStore;

struct LoginField {
    std::string name;
    std::string type;
    std::string label;

    bool required = false;

    JsonObject options;
};

struct LoginAction {
    std::string name;
    std::string label;
    JsonObject options;
};

struct LoginFormSpec {
    std::string title;
    std::vector<LoginField> fields;
    std::vector<LoginAction> actions;
    JsonObject options;
};

struct LoginRequest {
    std::string identityType;
    std::string nameHint;
    Realm realmHint;

    Permission permission;

    std::optional<Credential> credential;

    bool interactive = false;

    JsonObject context;
};

struct LoginResult {
    LoginStatus status = LoginStatus::Unknown;

    std::optional<IdentitySet> identities;

    std::vector<std::string> requiredCredentialTypes;

    std::optional<LoginFormSpec> form;

    std::string error;
};

class IdentityService : public ICommandSupport {
  public:
    virtual ~IdentityService() = default;

    virtual std::string id() const = 0;

    virtual std::string identityType() const = 0;

    virtual std::vector<std::string> supportedCredentialTypes() const = 0;

    /** Realm types this service accepts (e.g. global, device, app). Empty = any. */
    virtual std::vector<std::string> supportedRealmTypes() const { return {}; }

    virtual bool canAutoLogin() const { return false; }

    virtual LoginResult login(const LoginRequest& request) = 0;

    virtual void logout(const Identity& identity) = 0;

    virtual std::optional<IdentitySet> refresh(const Identity& identity) {
        return std::nullopt;
    }

    /** User stores (or equivalent) backing this service; empty when none. */
    virtual std::vector<std::shared_ptr<UserStore>> getBackedStores() const { return {}; }

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;
};

class AnonymousIdentityService : public IdentityService {
  public:
    std::string id() const override;
    std::string identityType() const override;
    std::vector<std::string> supportedCredentialTypes() const override;
    std::vector<std::string> supportedRealmTypes() const override;
    bool canAutoLogin() const override;
    LoginResult login(const LoginRequest& request) override;
    void logout(const Identity& identity) override;
};

/** Password login backed by a UserStore (profiles, roles, password-hash keys). */
class StoreIdentityService : public IdentityService {
  public:
    explicit StoreIdentityService(std::shared_ptr<UserStore> userStore);

    void setUserStore(std::shared_ptr<UserStore> userStore);

    const UserStore* userStore() const { return m_userStore.get(); }

    std::vector<std::shared_ptr<UserStore>> getBackedStores() const override;

    std::string id() const override;
    std::string identityType() const override;
    std::vector<std::string> supportedCredentialTypes() const override;
    std::vector<std::string> supportedRealmTypes() const override;
    LoginResult login(const LoginRequest& request) override;
    void logout(const Identity& identity) override;

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    std::shared_ptr<UserStore> m_userStore;
};

} // namespace bas::security

#endif
