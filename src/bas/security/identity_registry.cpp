#include "identity_registry.hpp"

#include <algorithm>

namespace bas::security {

void IdentityRegistry::add(std::shared_ptr<IdentityService> service) {
    m_services.push_back(std::move(service));
}

void IdentityRegistry::remove(const std::string& serviceId) {
    m_services.erase(std::remove_if(m_services.begin(), m_services.end(),
                                   [&](const std::shared_ptr<IdentityService>& s) {
                                       return s && s->id() == serviceId;
                                   }),
                      m_services.end());
    m_realmBindings.erase(
        std::remove_if(m_realmBindings.begin(), m_realmBindings.end(),
                       [&](const RealmBinding& binding) {
                           return binding.service && binding.service->id() == serviceId;
                       }),
        m_realmBindings.end());
    m_realms.clear();
    for (const auto& binding : m_realmBindings) {
        const bool alreadyListed = std::any_of(
            m_realms.begin(), m_realms.end(),
            [&](const Realm& realm) { return realmScopedEqual(realm, binding.realm); });
        if (!alreadyListed) {
            m_realms.push_back(binding.realm);
        }
    }
}

void IdentityRegistry::registerRealm(const Realm& realm,
                                     std::shared_ptr<IdentityService> service) {
    if (realm.empty() || !service) {
        return;
    }
    for (auto& binding : m_realmBindings) {
        if (realmScopedEqual(binding.realm, realm)) {
            binding.service = std::move(service);
            return;
        }
    }
    m_realmBindings.push_back(RealmBinding{realm, std::move(service)});
    m_realms.push_back(realm);
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::list() const { return m_services; }

std::optional<Realm> IdentityRegistry::findRealmByName(const std::string& name) const {
    if (name.empty()) {
        return std::nullopt;
    }
    for (const auto& realm : m_realms) {
        if (realm.name == name) {
            return realm;
        }
    }
    return std::nullopt;
}

Realm IdentityRegistry::resolveRealmHint(Realm hint) const {
    if (!hint.name.empty()) {
        if (auto registered = findRealmByName(hint.name)) {
            Realm resolved = *registered;
            if (!hint.type.empty()) {
                resolved.type = hint.type;
            }
            if (!hint.uuid.empty()) {
                resolved.uuid = hint.uuid;
            }
            return resolved;
        }
    }
    return hint;
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::findByIdentityType(
    const std::string& type) const {
    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& service : m_services) {
        if (service && service->identityType() == type) {
            found.push_back(service);
        }
    }
    return found;
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::findByCredentialType(
    const std::string& credentialType) const {
    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& service : m_services) {
        if (!service) {
            continue;
        }
        const auto types = service->supportedCredentialTypes();
        if (std::find(types.begin(), types.end(), credentialType) != types.end()) {
            found.push_back(service);
        }
    }
    return found;
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::findAutoLoginServices() const {
    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& service : m_services) {
        if (service && service->canAutoLogin()) {
            found.push_back(service);
        }
    }
    return found;
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::findByRealm(
    const Realm& realm) const {
    if (realm.empty() || !realm.hasKey()) {
        return m_services;
    }

    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& binding : m_realmBindings) {
        if (!binding.service) {
            continue;
        }
        if (realmMatches(binding.realm, realm) || realmScopedEqual(binding.realm, realm)) {
            found.push_back(binding.service);
        }
    }

    std::vector<std::shared_ptr<IdentityService>> unique;
    for (const auto& service : found) {
        if (std::none_of(unique.begin(), unique.end(),
                         [&](const std::shared_ptr<IdentityService>& existing) {
                             return existing && service && existing->id() == service->id();
                         })) {
            unique.push_back(service);
        }
    }
    return unique;
}

} // namespace bas::security
