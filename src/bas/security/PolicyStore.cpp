#include "PolicyStore.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <chrono>

namespace bas::security {

namespace {

bool identityEquals(const IdentityRef& a, const IdentityRef& b) { return a == b; }

} // namespace

bool DefaultPolicyStore::hasAcl(const std::string& aclId) const {
    return getAcl(aclId).has_value();
}

std::optional<ACList> DefaultPolicyStore::getAcl(const std::string& aclId) const {
    for (const auto& acl : m_acls) {
        if (acl.id == aclId) {
            return acl;
        }
    }
    return std::nullopt;
}

std::vector<std::string> DefaultPolicyStore::listAcls() const {
    std::vector<std::string> ids;
    ids.reserve(m_acls.size());
    for (const auto& acl : m_acls) {
        ids.push_back(acl.id);
    }
    return ids;
}

void DefaultPolicyStore::addAcl(ACList acl) {
    removeAcl(acl.id);
    m_acls.push_back(std::move(acl));
}

void DefaultPolicyStore::updateAcl(ACList acl) {
    for (auto& existing : m_acls) {
        if (existing.id == acl.id) {
            existing = std::move(acl);
            return;
        }
    }
    m_acls.push_back(std::move(acl));
}

void DefaultPolicyStore::removeAcl(const std::string& aclId) {
    m_acls.erase(std::remove_if(m_acls.begin(), m_acls.end(),
                                [&](const ACList& acl) { return acl.id == aclId; }),
                 m_acls.end());
    m_bindings.erase(std::remove_if(m_bindings.begin(), m_bindings.end(),
                                    [&](const PolicyBinding& binding) {
                                        return binding.aclId == aclId;
                                    }),
                     m_bindings.end());
}

std::vector<AccessGrant> DefaultPolicyStore::grantsOf(const IdentityRef& identity) const {
    std::vector<AccessGrant> found;
    for (const auto& grant : m_grants) {
        if (identityEquals(grant.identity, identity)) {
            found.push_back(grant);
        }
    }
    return found;
}

void DefaultPolicyStore::addGrant(AccessGrant grant) { m_grants.push_back(std::move(grant)); }

void DefaultPolicyStore::removeGrant(const AccessGrant& grant) {
    m_grants.erase(std::remove(m_grants.begin(), m_grants.end(), grant), m_grants.end());
}

std::vector<PolicyBinding> DefaultPolicyStore::bindingsOf(const IdentityRef& identity) const {
    std::vector<PolicyBinding> found;
    for (const auto& binding : m_bindings) {
        if (identityEquals(binding.identity, identity)) {
            found.push_back(binding);
        }
    }
    return found;
}

void DefaultPolicyStore::addBinding(PolicyBinding binding) {
    m_bindings.push_back(std::move(binding));
}

void DefaultPolicyStore::removeBinding(const PolicyBinding& binding) {
    m_bindings.erase(std::remove(m_bindings.begin(), m_bindings.end(), binding), m_bindings.end());
}

std::vector<ACEntry> DefaultPolicyStore::effectiveEntriesOf(const IdentityRef& identity) const {
    std::vector<ACEntry> entries;
    for (const auto& grant : grantsOf(identity)) {
        entries.push_back(grant.ace());
    }
    for (const auto& binding : bindingsOf(identity)) {
        const auto acl = getAcl(binding.aclId);
        if (!acl.has_value()) {
            continue;
        }
        entries.insert(entries.end(), acl->entries.begin(), acl->entries.end());
    }
    return entries;
}

void DefaultPolicyStore::clear() {
    m_acls.clear();
    m_grants.clear();
    m_bindings.clear();
}

void DefaultPolicyStore::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    clear();

    const auto loadAclArray = [&](const boost::json::array& arr) {
        for (auto& item : arr) {
            if (!item.is_object()) {
                continue;
            }
            ACList acl;
            acl.jsonIn(item.as_object(), opts);
            if (!acl.id.empty()) {
                addAcl(std::move(acl));
            }
        }
    };

    if (auto* acls = o.if_contains("aclists")) {
        if (acls->is_array()) {
            loadAclArray(acls->as_array());
        }
    } else if (auto* policies = o.if_contains("policies")) {
        if (policies->is_array()) {
            loadAclArray(policies->as_array());
        }
    }

    if (auto* bindings = o.if_contains("bindings")) {
        if (bindings->is_array()) {
            for (auto& item : bindings->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                PolicyBinding binding;
                binding.jsonIn(item.as_object(), opts);
                if (!binding.aclId.empty()) {
                    addBinding(std::move(binding));
                }
            }
        }
    }

    if (auto* grants = o.if_contains("grants")) {
        if (grants->is_array()) {
            for (auto& item : grants->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                AccessGrant grant;
                grant.jsonIn(item.as_object(), opts);
                addGrant(std::move(grant));
            }
        }
    } else if (auto* entries = o.if_contains("entries")) {
        if (entries->is_array()) {
            for (auto& item : entries->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                AccessGrant grant;
                grant.jsonIn(item.as_object(), opts);
                addGrant(std::move(grant));
            }
        }
    }
}

void DefaultPolicyStore::jsonOut(boost::json::object& o, const JsonFormOptions& opts) {
    if (!m_acls.empty()) {
        boost::json::array acls;
        for (const auto& acl : m_acls) {
            boost::json::object item;
            ACList copy = acl;
            copy.jsonOut(item, opts);
            acls.push_back(item);
        }
        o["aclists"] = acls;
    }

    if (!m_bindings.empty()) {
        boost::json::array bindings;
        for (const auto& binding : m_bindings) {
            boost::json::object item;
            binding.jsonOut(item, opts);
            bindings.push_back(item);
        }
        o["bindings"] = bindings;
    }

    if (!m_grants.empty()) {
        boost::json::array grants;
        for (const auto& grant : m_grants) {
            boost::json::object item;
            grant.jsonOut(item, opts);
            grants.push_back(item);
        }
        o["grants"] = grants;
    }
}

FilePolicyStore::FilePolicyStore(VolumeFile file, std::shared_ptr<PolicyStore> wrapped)
    : DecoratedPolicyStore(std::move(wrapped)), m_file(std::move(file)) {
    loadFromDisk();
}

FilePolicyStore::FilePolicyStore(std::filesystem::path path, std::shared_ptr<PolicyStore> wrapped)
    : DecoratedPolicyStore(std::move(wrapped)),
      m_ownedVolume(std::make_shared<LocalVolume>(
          path.parent_path().empty() ? "." : path.parent_path().string())),
      m_file(m_ownedVolume, path.filename().string()) {
    loadFromDisk();
}

std::filesystem::path FilePolicyStore::storePath() const {
    return std::filesystem::path(m_file.getLocalFile());
}

bool FilePolicyStore::canPersistToDisk() const { return true; }

void FilePolicyStore::reloadFromDisk() { loadFromDisk(); }

void FilePolicyStore::persistToDisk() { saveToDisk(); }

void FilePolicyStore::addGrant(AccessGrant grant) {
    DecoratedPolicyStore::addGrant(std::move(grant));
    saveToDisk();
}

void FilePolicyStore::removeGrant(const AccessGrant& grant) {
    DecoratedPolicyStore::removeGrant(grant);
    saveToDisk();
}

void FilePolicyStore::clear() {
    DecoratedPolicyStore::clear();
    saveToDisk();
}

void FilePolicyStore::addAcl(ACList acl) {
    DecoratedPolicyStore::addAcl(std::move(acl));
    saveToDisk();
}

void FilePolicyStore::removeAcl(const std::string& aclId) {
    DecoratedPolicyStore::removeAcl(aclId);
    saveToDisk();
}

void FilePolicyStore::addBinding(PolicyBinding binding) {
    DecoratedPolicyStore::addBinding(std::move(binding));
    saveToDisk();
}

void FilePolicyStore::removeBinding(const PolicyBinding& binding) {
    DecoratedPolicyStore::removeBinding(binding);
    saveToDisk();
}

void FilePolicyStore::loadFromDisk() {
    wrapped()->clear();
    if (!m_file.exists() || !m_file.isFile()) {
        return;
    }
    const auto content = m_file.readFileUTF8Opt(std::nullopt);
    if (!content.has_value() || content->empty()) {
        return;
    }
    boost::json::value root;
    try {
        root = boost::json::parse(*content);
    } catch (const std::exception&) {
        return;
    }
    if (!root.is_object()) {
        return;
    }
    auto* store = dynamic_cast<DefaultPolicyStore*>(wrapped());
    if (store != nullptr) {
        store->jsonIn(root.as_object(), JsonFormOptions::DEFAULT);
    }
}

void FilePolicyStore::saveToDisk() {
    m_file.createParentDirectories();
    boost::json::object envelope;
    auto* store = dynamic_cast<DefaultPolicyStore*>(wrapped());
    if (store != nullptr) {
        store->jsonOut(envelope, JsonFormOptions::DEFAULT);
    }
    m_file.writeFileString(boost::json::serialize(envelope), "UTF-8");
}

namespace {

const PolicyStore* resolvePolicyStore(const PolicyStore& store, const IdentityRef& identity) {
    if (const auto* realmStore = dynamic_cast<const RealmPolicyStore*>(&store)) {
        if (const auto resolved = realmStore->storeForRealm(identity.realm)) {
            return resolved.get();
        }
    }
    return &store;
}

} // namespace

void RealmPolicyStore::addRealmStore(const Realm& realm, std::shared_ptr<PolicyStore> store) {
    if (!store || !realm.hasKey()) {
        return;
    }
    const std::string key = realm.storageKey();
    m_byKey[key] = std::move(store);
    m_keyToRealm[key] = realm;
    m_realmKeys.clear();
    for (const auto& [k, r] : m_keyToRealm) {
        (void)k;
        m_realmKeys.push_back(r);
    }
}

std::shared_ptr<PolicyStore> RealmPolicyStore::storeForRealm(const Realm& realm) const {
    if (realm.hasKey()) {
        const auto it = m_byKey.find(realm.storageKey());
        if (it != m_byKey.end()) {
            return it->second;
        }
    }
    return m_global;
}

const PolicyStore* RealmPolicyStore::resolveFor(const IdentityRef& identity) const {
    if (identity.realm.hasKey()) {
        const auto it = m_byKey.find(identity.realm.storageKey());
        if (it != m_byKey.end()) {
            return it->second.get();
        }
    }
    return m_global.get();
}

PolicyStore* RealmPolicyStore::resolveFor(const IdentityRef& identity) {
    if (identity.realm.hasKey()) {
        const auto it = m_byKey.find(identity.realm.storageKey());
        if (it != m_byKey.end()) {
            return it->second.get();
        }
    }
    return m_global.get();
}

bool RealmPolicyStore::hasAcl(const std::string& aclId) const {
    for (const auto& [key, store] : m_byKey) {
        (void)key;
        if (store && store->hasAcl(aclId)) {
            return true;
        }
    }
    return m_global && m_global->hasAcl(aclId);
}

std::optional<ACList> RealmPolicyStore::getAcl(const std::string& aclId) const {
    for (const auto& [key, store] : m_byKey) {
        (void)key;
        if (store) {
            if (auto acl = store->getAcl(aclId)) {
                return acl;
            }
        }
    }
    if (m_global) {
        return m_global->getAcl(aclId);
    }
    return std::nullopt;
}

std::vector<std::string> RealmPolicyStore::listAcls() const {
    std::vector<std::string> ids;
    for (const auto& [key, store] : m_byKey) {
        (void)key;
        if (!store) {
            continue;
        }
        for (const auto& id : store->listAcls()) {
            ids.push_back(id);
        }
    }
    if (m_global) {
        for (const auto& id : m_global->listAcls()) {
            ids.push_back(id);
        }
    }
    return ids;
}

void RealmPolicyStore::addAcl(ACList acl) {
    if (m_global) {
        m_global->addAcl(std::move(acl));
    }
}

void RealmPolicyStore::updateAcl(ACList acl) {
    if (m_global) {
        m_global->updateAcl(std::move(acl));
    }
}

void RealmPolicyStore::removeAcl(const std::string& aclId) {
    for (auto& [key, store] : m_byKey) {
        (void)key;
        if (store) {
            store->removeAcl(aclId);
        }
    }
    if (m_global) {
        m_global->removeAcl(aclId);
    }
}

std::vector<AccessGrant> RealmPolicyStore::grantsOf(const IdentityRef& identity) const {
    if (const auto* store = resolveFor(identity)) {
        return store->grantsOf(identity);
    }
    return {};
}

void RealmPolicyStore::addGrant(AccessGrant grant) {
    if (auto* store = resolveFor(grant.identity)) {
        store->addGrant(std::move(grant));
    } else if (m_global) {
        m_global->addGrant(std::move(grant));
    }
}

void RealmPolicyStore::removeGrant(const AccessGrant& grant) {
    if (auto* store = resolveFor(grant.identity)) {
        store->removeGrant(grant);
    } else if (m_global) {
        m_global->removeGrant(grant);
    }
}

std::vector<PolicyBinding> RealmPolicyStore::bindingsOf(const IdentityRef& identity) const {
    if (const auto* store = resolveFor(identity)) {
        return store->bindingsOf(identity);
    }
    return {};
}

void RealmPolicyStore::addBinding(PolicyBinding binding) {
    if (auto* store = resolveFor(binding.identity)) {
        store->addBinding(std::move(binding));
    } else if (m_global) {
        m_global->addBinding(std::move(binding));
    }
}

void RealmPolicyStore::removeBinding(const PolicyBinding& binding) {
    if (auto* store = resolveFor(binding.identity)) {
        store->removeBinding(binding);
    } else if (m_global) {
        m_global->removeBinding(binding);
    }
}

std::vector<ACEntry> RealmPolicyStore::effectiveEntriesOf(const IdentityRef& identity) const {
    if (const auto* store = resolveFor(identity)) {
        return store->effectiveEntriesOf(identity);
    }
    return {};
}

void RealmPolicyStore::clear() {
    m_byKey.clear();
    m_keyToRealm.clear();
    m_realmKeys.clear();
    m_grantsCache.clear();
    if (m_global) {
        m_global->clear();
    }
}

const std::vector<AccessGrant>& RealmPolicyStore::grants() const {
    m_grantsCache.clear();
    for (const auto& [key, store] : m_byKey) {
        (void)key;
        if (!store) {
            continue;
        }
        const auto& grants = store->grants();
        m_grantsCache.insert(m_grantsCache.end(), grants.begin(), grants.end());
    }
    if (m_global) {
        const auto& grants = m_global->grants();
        m_grantsCache.insert(m_grantsCache.end(), grants.begin(), grants.end());
    }
    return m_grantsCache;
}

void RealmPolicyStore::jsonIn(const boost::json::object& /*o*/, const JsonFormOptions& /*opts*/) {}

void RealmPolicyStore::jsonOut(boost::json::object& /*o*/, const JsonFormOptions& /*opts*/) {}

std::vector<ACEMatch> policyFind(const PolicyStore& store, const IdentityRef& identity,
                                const Permission& permission, const PermissionMatcher& matcher) {
    std::vector<ACEMatch> matches;
    const PolicyStore& resolved = *resolvePolicyStore(store, identity);
    for (const auto& entry : resolved.effectiveEntriesOf(identity)) {
        if (!matcher.matches(entry.permission, permission)) {
            continue;
        }
        ACEMatch match;
        match.entry = entry;
        match.identity = identity;
        match.permissionSpecificity = matcher.specificity(entry.permission);
        match.identitySpecificity = 10;
        matches.push_back(match);
    }
    return matches;
}

AccessEffect policyCheck(const PolicyStore& store, const IdentityRef& identity,
                         const Permission& permission, const PermissionMatcher& matcher,
                         const ACResolvePolicy& resolver) {
    return resolver.resolve(policyFind(store, identity, permission, matcher));
}

AccessEffect policyCheckAny(const PolicyStore& store, const std::vector<Identity>& identities,
                            const Permission& permission, const PermissionMatcher& matcher,
                            const ACResolvePolicy& resolver) {
    const auto now = std::chrono::system_clock::now();
    std::vector<ACEMatch> all;
    for (const auto& identity : identities) {
        if (!identity.isActive(now)) {
            continue;
        }
        auto matches = policyFind(store, identity.ref(), permission, matcher);
        for (auto& match : matches) {
            match.identitySpecificity += identity.source.priority();
            all.push_back(match);
        }
    }
    return resolver.resolve(all);
}

} // namespace bas::security
