#ifndef BAS_SECURITY_IDENTITY_REGISTRY_HPP
#define BAS_SECURITY_IDENTITY_REGISTRY_HPP

#include "CommandSupport.hpp"
#include "IdentityService.hpp"
#include "Realm.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace bas::security {

/** Realm → identity-service bindings plus optional unbound services (no realm slot). */
class IdentityRegistry : public ICommandSupport {
  public:
    /**
     * Bind @a service to @a realm. Returns false when the realm slot is already taken by
     * another service. When @a realm is omitted or has no key, registers @a service as
     * unbound (discoverable via @ref list but not tied to a realm slot).
     */
    bool add(std::shared_ptr<IdentityService> service, std::optional<Realm> realm = std::nullopt);

    /** Remove every realm slot bound to @a service. Returns the number of slots removed. */
    std::size_t remove(const std::shared_ptr<IdentityService>& service);

    /** Remove the realm slot for @a realm. Returns true when a slot was removed. */
    bool remove(const Realm& realm);

    /** Registered realm slots (map keys). */
    const std::vector<Realm>& realms() const { return m_realmKeys; }

    /** Realm slots currently bound to @a service. */
    std::vector<Realm> realms(const std::shared_ptr<IdentityService>& service) const;

    bool contains(const Realm& realm) const;

    /** Service bound to @a realm, if any. */
    std::shared_ptr<IdentityService> load(const Realm& realm) const;

    /** Unique services: all bound values plus unbound registrations. */
    std::vector<std::shared_ptr<IdentityService>> list() const;

    std::optional<Realm> findRealmByName(const std::string& name) const;

    Realm resolveRealmHint(Realm hint) const;

    std::vector<std::shared_ptr<IdentityService>> findByIdentityType(const std::string& type) const;

    std::vector<std::shared_ptr<IdentityService>>
    findByCredentialType(const std::string& credentialType) const;

    std::vector<std::shared_ptr<IdentityService>> findAutoLoginServices() const;

    /** Services for @a realm hint; all services when @a realm is empty. */
    std::vector<std::shared_ptr<IdentityService>> findByRealm(const Realm& realm) const;

    std::shared_ptr<IdentityService> findById(const std::string& serviceId) const;

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    struct RealmHash {
        std::size_t operator()(const Realm& realm) const {
            return std::hash<std::string>{}(realm.storageKey());
        }
    };

    struct RealmEqual {
        bool operator()(const Realm& lhs, const Realm& rhs) const { return lhs.scopedEqual(rhs); }
    };

    using BindingMap =
        std::unordered_map<Realm, std::shared_ptr<IdentityService>, RealmHash, RealmEqual>;

    std::optional<BindingMap::const_iterator> findBinding(const Realm& realm) const;

    void rebuildRealmKeys();

    std::vector<std::shared_ptr<IdentityService>> uniqueServices() const;

    BindingMap m_bindings;
    std::vector<std::shared_ptr<IdentityService>> m_unbound;
    std::vector<Realm> m_realmKeys;
};

} // namespace bas::security

#endif
