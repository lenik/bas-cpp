#ifndef BAS_SECURITY_IDENTITY_REGISTRY_HPP
#define BAS_SECURITY_IDENTITY_REGISTRY_HPP

#include "command_support.hpp"
#include "identity_service.hpp"
#include "realm.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

/** Registry of identity services and explicit realm → service bindings. */
class IdentityRegistry : public ICommandSupport {
  public:
    void add(std::shared_ptr<IdentityService> service);

    void remove(const std::string& serviceId);

    /** Bind @a realm to @a service (replaces an existing binding for the same realm slot). */
    void registerRealm(const Realm& realm, std::shared_ptr<IdentityService> service);

    std::vector<std::shared_ptr<IdentityService>> list() const;

    /** Realms registered via @ref registerRealm. */
    const std::vector<Realm>& realms() const { return m_realms; }

    std::optional<Realm> findRealmByName(const std::string& name) const;

    Realm resolveRealmHint(Realm hint) const;

    std::vector<std::shared_ptr<IdentityService>> findByIdentityType(
        const std::string& type) const;

    std::vector<std::shared_ptr<IdentityService>> findByCredentialType(
        const std::string& credentialType) const;

    std::vector<std::shared_ptr<IdentityService>> findAutoLoginServices() const;

    /** Services bound to @a realm; all services when @a realm is empty. */
    std::vector<std::shared_ptr<IdentityService>> findByRealm(const Realm& realm) const;

    std::shared_ptr<IdentityService> findById(const std::string& serviceId) const;

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    struct RealmBinding {
        Realm realm;
        std::shared_ptr<IdentityService> service;
    };

    std::vector<std::shared_ptr<IdentityService>> m_services;
    std::vector<RealmBinding> m_realmBindings;
    std::vector<Realm> m_realms;
};

} // namespace bas::security

#endif
