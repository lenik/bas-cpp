#ifndef BAS_SECURITY_LOGIN_UI_HPP
#define BAS_SECURITY_LOGIN_UI_HPP

#include "CredentialManager.hpp"
#include "IdentityService.hpp"
#include "Realm.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace bas::security {

class SecurityManager;

/** Build a credential from form field values supplied by @a readField (return nullopt to cancel). */
std::optional<Credential> credentialFromLoginForm(
    const LoginFormSpec& form, const CredentialRequest& request,
    const std::function<std::optional<std::string>(const LoginField& field)>& readField);

class LoginUi {
  public:
    virtual ~LoginUi() = default;

    virtual SecurityManager* controller() = 0;

    virtual std::optional<std::string> realmType() const { return std::nullopt; }

    virtual std::optional<Realm> realm() const { return std::nullopt; }

    virtual std::optional<Credential> requestCredential(const LoginFormSpec& form,
                                                        const CredentialRequest& request) = 0;
};

class ConsoleLogin : public LoginUi {
  public:
    explicit ConsoleLogin(SecurityManager& controller);

    SecurityManager* controller() override;

    std::optional<std::string> realmType() const override;

    std::optional<Realm> realm() const override;

    std::optional<Credential> requestCredential(const LoginFormSpec& form,
                                                const CredentialRequest& request) override;

  private:
    SecurityManager& m_controller;
};

} // namespace bas::security

#endif
