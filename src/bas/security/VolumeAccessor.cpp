#include "VolumeAccessor.hpp"

#include "AccessDecisionResolver.hpp"
#include "AccessDenied.hpp"
#include "Identity.hpp"
#include "Permission.hpp"
#include "PolicyStore.hpp"
#include "PublicAccess.hpp"
#include "SecurityManager.hpp"
#include "Types.hpp"

#include "../io/InputStream.hpp"
#include "../io/OutputStream.hpp"

#include <stdexcept>
#include <utility>

namespace bas::security {

Realm realmForVolume(Volume* volume) {
    Realm realm;
    realm.type = "volume";
    if (!volume)
        return realm;
    realm.uuid = volume->getUuid();
    if (realm.uuid.empty())
        realm.name = volume->getUrl();
    else
        realm.name = volume->getLabel();
    return realm;
}

VolumeAccessor::VolumeAccessor(Volume* inner, SecurityManager* securityManager)
    : m_inner(inner), m_sm(securityManager), m_realm(realmForVolume(inner)) {
    if (!m_inner)
        throw std::invalid_argument("VolumeAccessor requires an inner Volume");
    if (!m_sm)
        throw std::invalid_argument("VolumeAccessor requires a SecurityManager");
}

void VolumeAccessor::setElevationSubject(std::optional<Subject> subject) {
    m_elevation = std::move(subject);
}

void VolumeAccessor::clearElevation() { m_elevation.reset(); }

Permission VolumeAccessor::perm(std::string_view action, std::string_view resource) const {
    Permission p;
    p.action = std::string(action);
    p.resource = std::string(resource);
    return p;
}

std::vector<Identity> VolumeAccessor::authorizationIdentities() const {
    std::vector<Identity> source;
    if (m_elevation.has_value()) {
        source = m_elevation->identities;
    } else {
        source = m_sm->currentIdentities();
    }

    std::vector<Identity> filtered;
    filtered.reserve(source.size());
    for (const auto& id : source) {
        // Auto / non-login identities apply across volumes (PublicAccess anonymous).
        if (!isLoginSessionIdentity(id)) {
            filtered.push_back(id);
            continue;
        }
        if (!m_realm.hasKey() || id.realm.match(m_realm) || id.realm.same(m_realm))
            filtered.push_back(id);
    }
    return filtered;
}

AccessEffect VolumeAccessor::check(const Permission& permission) const {
    static DefaultPermissionMatcher matcher;
    static DefaultACResolvePolicy resolver;
    const auto identities = authorizationIdentities();
    auto store = const_cast<VolumeAccessor*>(this)->getPolicyStore();
    AccessEffect effect = policyCheckAny(*store, identities, permission, matcher, resolver);
    if (effect.isUnknown())
        return AccessEffect::Deny;
    return effect;
}

void VolumeAccessor::require(const Permission& permission) const {
    if (check(permission) != AccessEffect::Allow)
        throw AccessDenied(permission);
}

std::shared_ptr<UserStore> VolumeAccessor::getUserStore() { return m_inner->getUserStore(); }

std::shared_ptr<PolicyStore> VolumeAccessor::getPolicyStore() { return m_inner->getPolicyStore(); }

std::string VolumeAccessor::getClass() const { return m_inner->getClass(); }
std::string VolumeAccessor::getUrl() const { return m_inner->getUrl(); }
std::string VolumeAccessor::getDeviceUrl() const { return m_inner->getDeviceUrl(); }
VolumeType VolumeAccessor::getType() const { return m_inner->getType(); }

std::string VolumeAccessor::readUuid() { return m_inner->readUuid(); }
std::string VolumeAccessor::readLabel() { return m_inner->readLabel(); }
bool VolumeAccessor::writeUuid(std::string_view uuid) { return m_inner->writeUuid(uuid); }
bool VolumeAccessor::writeLabel(std::string_view label) { return m_inner->writeLabel(label); }

bool VolumeAccessor::isEncrypted() const { return m_inner->isEncrypted(); }
bool VolumeAccessor::isLocal() const { return m_inner->isLocal(); }
std::optional<std::string> VolumeAccessor::getLocalFile(std::string_view path) const {
    return m_inner->getLocalFile(path);
}
std::string VolumeAccessor::getDefaultLabel() const { return m_inner->getLabel(); }

bool VolumeAccessor::exists(std::string_view path) const { return m_inner->exists(path); }
bool VolumeAccessor::isFile(std::string_view path) const { return m_inner->isFile(path); }
bool VolumeAccessor::isDirectory(std::string_view path) const { return m_inner->isDirectory(path); }
bool VolumeAccessor::stat(std::string_view path, DirNode* status) const {
    return m_inner->stat(path, status);
}

void VolumeAccessor::readDir_inplace(DirNode& context, std::string_view path, bool recursive) {
    require(perm("list", path));
    m_inner->readDir_inplace(context, path, recursive);
}

std::unique_ptr<InputStream> VolumeAccessor::newInputStream(std::string_view path) {
    require(perm("read", path));
    return m_inner->newInputStream(path);
}

std::unique_ptr<OutputStream> VolumeAccessor::newOutputStream(std::string_view path, bool append) {
    require(perm("write", path));
    return m_inner->newOutputStream(path, append);
}

std::unique_ptr<RandomInputStream> VolumeAccessor::newRandomInputStream(std::string_view path) {
    require(perm("read", path));
    return m_inner->newRandomInputStream(path);
}

std::string VolumeAccessor::getTempDir() { return m_inner->getTempDir(); }

std::string VolumeAccessor::createTempFile(std::string_view prefix, std::string_view suffix) {
    return m_inner->createTempFile(prefix, suffix);
}

std::vector<uint8_t> VolumeAccessor::readFileUnchecked(std::string_view path, int64_t off,
                                                       size_t len) {
    require(perm("read", path));
    return m_inner->readFileUnchecked(path, off, len);
}

void VolumeAccessor::writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) {
    require(perm("write", path));
    m_inner->writeFileUnchecked(path, data);
}

void VolumeAccessor::createDirectoryThrowsUnchecked(std::string_view path) {
    require(perm("write", path));
    m_inner->createDirectoryThrowsUnchecked(path);
}

void VolumeAccessor::removeDirectoryThrowsUnchecked(std::string_view path) {
    require(perm("delete", path));
    m_inner->removeDirectoryThrowsUnchecked(path);
}

void VolumeAccessor::removeFileThrowsUnchecked(std::string_view path) {
    require(perm("delete", path));
    m_inner->removeFileThrowsUnchecked(path);
}

void VolumeAccessor::copyFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    require(perm("read", src));
    require(perm("write", dest));
    m_inner->copyFileThrowsUnchecked(src, dest);
}

void VolumeAccessor::moveFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    require(perm("read", src));
    require(perm("write", dest));
    require(perm("delete", src));
    m_inner->moveFileThrowsUnchecked(src, dest);
}

void VolumeAccessor::renameFileThrowsUnchecked(std::string_view src, std::string_view dest) {
    require(perm("read", src));
    require(perm("write", dest));
    require(perm("delete", src));
    m_inner->renameFileThrowsUnchecked(src, dest);
}

} // namespace bas::security
