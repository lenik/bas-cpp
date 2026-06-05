#include "IdentityRegistry.hpp"

#include <algorithm>

namespace bas::security {

namespace {

bool sameService(const std::shared_ptr<IdentityService>& a,
                 const std::shared_ptr<IdentityService>& b) {
    if (!a || !b) {
        return false;
    }
    return a->id() == b->id();
}

} // namespace

std::optional<IdentityRegistry::BindingMap::const_iterator>
IdentityRegistry::findBinding(const Realm& realm) const {
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        if (it->first.scopedEqual(realm)) {
            return it;
        }
    }
    return std::nullopt;
}

bool IdentityRegistry::add(std::shared_ptr<IdentityService> service, std::optional<Realm> realm) {
    if (!service) {
        return false;
    }

    if (!realm || realm->empty() || !realm->hasKey()) {
        const auto existing = std::find_if(
            m_unbound.begin(), m_unbound.end(),
            [&](const std::shared_ptr<IdentityService>& s) { return sameService(s, service); });
        if (existing != m_unbound.end()) {
            return true;
        }
        m_unbound.push_back(service);
        return true;
    }

    if (const auto bound = findBinding(*realm)) {
        if (!sameService(bound.value()->second, service)) {
            return false;
        }
        return true;
    }

    m_bindings.emplace(*realm, std::move(service));
    rebuildRealmKeys();
    return true;
}

std::size_t IdentityRegistry::remove(const std::shared_ptr<IdentityService>& service) {
    if (!service) {
        return 0;
    }

    std::size_t removed = 0;
    for (auto it = m_bindings.begin(); it != m_bindings.end();) {
        if (sameService(it->second, service)) {
            it = m_bindings.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }

    m_unbound.erase(std::remove_if(m_unbound.begin(), m_unbound.end(),
                                   [&](const std::shared_ptr<IdentityService>& s) {
                                       return sameService(s, service);
                                   }),
                    m_unbound.end());

    if (removed != 0) {
        rebuildRealmKeys();
    }
    return removed;
}

bool IdentityRegistry::remove(const Realm& realm) {
    if (!realm.hasKey()) {
        return false;
    }
    if (const auto bound = findBinding(realm)) {
        m_bindings.erase(bound.value());
        rebuildRealmKeys();
        return true;
    }
    return false;
}

std::vector<Realm> IdentityRegistry::realms(const std::shared_ptr<IdentityService>& service) const {
    std::vector<Realm> found;
    if (!service) {
        return found;
    }
    for (const auto& [realm, bound] : m_bindings) {
        if (sameService(bound, service)) {
            found.push_back(realm);
        }
    }
    return found;
}

bool IdentityRegistry::contains(const Realm& realm) const {
    if (!realm.hasKey()) {
        return false;
    }
    return findBinding(realm).has_value();
}

std::shared_ptr<IdentityService> IdentityRegistry::load(const Realm& realm) const {
    if (!realm.hasKey()) {
        return nullptr;
    }
    if (const auto bound = findBinding(realm)) {
        return bound.value()->second;
    }
    return nullptr;
}

void IdentityRegistry::rebuildRealmKeys() {
    m_realmKeys.clear();
    m_realmKeys.reserve(m_bindings.size());
    for (const auto& [realm, service] : m_bindings) {
        (void)service;
        m_realmKeys.push_back(realm);
    }
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::uniqueServices() const {
    std::vector<std::shared_ptr<IdentityService>> services;
    auto appendUnique = [&](const std::shared_ptr<IdentityService>& service) {
        if (!service) {
            return;
        }
        if (std::none_of(services.begin(), services.end(),
                         [&](const std::shared_ptr<IdentityService>& existing) {
                             return sameService(existing, service);
                         })) {
            services.push_back(service);
        }
    };
    for (const auto& service : m_unbound) {
        appendUnique(service);
    }
    for (const auto& [realm, service] : m_bindings) {
        (void)realm;
        appendUnique(service);
    }
    return services;
}

std::vector<std::shared_ptr<IdentityService>> IdentityRegistry::list() const {
    return uniqueServices();
}

std::optional<Realm> IdentityRegistry::findRealmByName(const std::string& name) const {
    if (name.empty()) {
        return std::nullopt;
    }
    for (const auto& realm : m_realmKeys) {
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

std::vector<std::shared_ptr<IdentityService>>
IdentityRegistry::findByIdentityType(const std::string& type) const {
    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& service : uniqueServices()) {
        if (service && service->identityType() == type) {
            found.push_back(service);
        }
    }
    return found;
}

std::vector<std::shared_ptr<IdentityService>>
IdentityRegistry::findByCredentialType(const std::string& credentialType) const {
    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& service : uniqueServices()) {
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
    for (const auto& service : uniqueServices()) {
        if (service && service->canAutoLogin()) {
            found.push_back(service);
        }
    }
    return found;
}

std::vector<std::shared_ptr<IdentityService>>
IdentityRegistry::findByRealm(const Realm& realm) const {
    if (realm.empty() || !realm.hasKey()) {
        return uniqueServices();
    }

    std::vector<std::shared_ptr<IdentityService>> found;
    for (const auto& [boundRealm, service] : m_bindings) {
        if (!service) {
            continue;
        }
        if (boundRealm.match(realm) || boundRealm.scopedEqual(realm)) {
            found.push_back(service);
        }
    }

    std::vector<std::shared_ptr<IdentityService>> unique;
    for (const auto& service : found) {
        if (std::none_of(unique.begin(), unique.end(),
                         [&](const std::shared_ptr<IdentityService>& existing) {
                             return sameService(existing, service);
                         })) {
            unique.push_back(service);
        }
    }
    return unique;
}

std::shared_ptr<IdentityService> IdentityRegistry::findById(const std::string& serviceId) const {
    if (serviceId.empty()) {
        return nullptr;
    }
    for (const auto& service : uniqueServices()) {
        if (service && service->id() == serviceId) {
            return service;
        }
    }
    return nullptr;
}

} // namespace bas::security
