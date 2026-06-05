#include "UserStore.hpp"

#include "bas/script/json.hpp"
#include "PasswordDigest.hpp"
#include "Realm.hpp"

#include "../reg/JsonRegistry.hpp"
#include "../reg/Registry.hpp"
#include "../reg/Sub.hpp"
#include "../reg/variant.hpp"

#include "../time/Instant.hpp"
#include "../time/OffsetDateTime.hpp"
#include "../time/ZonedDateTime.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bas::security {

namespace {

std::optional<std::chrono::system_clock::time_point> parseOptionalTime(const boost::json::value& v) {
    if (!v.is_string()) {
        return std::nullopt;
    }
    const std::string s = v.as_string().c_str();
    if (time::Instant::isValidIsoString(s)) {
        return time::Instant::parseIsoString(s)->toTimePoint();
    }
    if (time::ZonedDateTime::isValidIsoString(s)) {
        return time::ZonedDateTime::parseIsoString(s)->toTimePoint();
    }
    if (time::OffsetDateTime::isValidIsoString(s)) {
        return time::OffsetDateTime::parseIsoString(s)->toTimePoint();
    }
    return std::nullopt;
}

void writeOptionalTime(boost::json::object& o, const char* key,
                       const std::optional<std::chrono::system_clock::time_point>& tp) {
    if (!tp.has_value()) {
        return;
    }
    const auto zdt = time::ZonedDateTime::fromTimePoint(*tp);
    o[key] = zdt->toIsoString();
}

JsonObject jsonObjectOrEmpty(const boost::json::object& o, const char* key) {
    if (auto* v = o.if_contains(key)) {
        if (v->is_object()) {
            return v->as_object();
        }
    }
    return {};
}

std::string jsonString(const boost::json::object& o, const char* key,
                       const std::string& defaultValue = {}) {
    return boost::json::get_string(o, key, defaultValue.c_str());
}

std::string normalizeRrlPrefix(std::string prefix) {
    while (!prefix.empty() && prefix.front() == '/') {
        prefix.erase(prefix.begin());
    }
    while (!prefix.empty() && prefix.back() == '/') {
        prefix.pop_back();
    }
    return prefix;
}

std::string joinRrlPath(std::string_view base, std::string_view name) {
    if (base.empty()) {
        return std::string(name);
    }
    std::string out(base);
    if (out.back() != '/') {
        out.push_back('/');
    }
    out.append(name);
    return out;
}

std::optional<boost::json::value> loadRegistryJson(::bas::reg::IRegistry& registry,
                                                     ::bas::reg::IContainerManager* containers,
                                                     std::string_view path) {
    if (containers != nullptr) {
        const auto rrls = ::bas::reg::RRL::parse(path, '/');
        for (const auto& rrl : rrls) {
            const boost::json::value* node = containers->cacheLoadFragment(rrl);
            if (node != nullptr) {
                return *node;
            }
        }
    }
    if (auto* jsonRegistry = dynamic_cast<::bas::reg::JsonRegistry*>(&registry)) {
        return jsonRegistry->getJson(path);
    }
    const ::bas::reg::option_t opt = registry.getOption(path);
    if (!opt.has_value()) {
        return std::nullopt;
    }
    if (const auto* serialized = std::get_if<std::string>(&*opt)) {
        try {
            return boost::json::parse(*serialized);
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return ::bas::reg::optionToJson(opt);
}

bool saveRegistryJson(::bas::reg::IRegistry& registry, ::bas::reg::IContainerManager* containers,
                      std::string_view path, const boost::json::value& value) {
    if (containers != nullptr) {
        const auto rrls = ::bas::reg::RRL::parse(path, '/');
        for (const auto& rrl : rrls) {
            if (rrl.hasFragment()) {
                return containers->cacheSaveFragment(rrl, value);
            }
        }
        for (const auto& rrl : rrls) {
            if (!rrl.hasFragment()) {
                containers->cacheSaveContainer(rrl.container, value);
                return true;
            }
        }
        return false;
    }
    if (auto* jsonRegistry = dynamic_cast<::bas::reg::JsonRegistry*>(&registry)) {
        return jsonRegistry->setJson(path, value);
    }
    return false;
}

bool removeRegistryJson(::bas::reg::IRegistry& registry, ::bas::reg::IContainerManager* containers,
                        std::string_view path) {
    if (containers != nullptr) {
        const auto rrls = ::bas::reg::RRL::parse(path, '/');
        for (const auto& rrl : rrls) {
            if (containers->cacheRemoveFragment(rrl)) {
                return true;
            }
        }
    }
    return registry.delTree(path);
}

std::optional<UserRecord> userRecordFromRegistryJson(const boost::json::value& json) {
    if (!json.is_object()) {
        return std::nullopt;
    }
    UserRecord record;
    record.jsonIn(json.as_object(), JsonFormOptions::DEFAULT);
    if (record.profile.name.empty()) {
        return std::nullopt;
    }
    return record;
}

} // namespace

void UserProfile::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    name = jsonString(o, "name");
    displayName = jsonString(o, "displayName");
    email = jsonString(o, "email");
    enabled = boost::json::get_bool(o, "enabled", true);
    attributes = jsonObjectOrEmpty(o, "attributes");
}

void UserProfile::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) {
    o["name"] = name;
    if (!displayName.empty()) {
        o["displayName"] = displayName;
    }
    if (!email.empty()) {
        o["email"] = email;
    }
    o["enabled"] = enabled;
    if (!attributes.empty()) {
        o["attributes"] = attributes;
    }
}

void UserAuthKey::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    id = jsonString(o, "id");
    type = jsonString(o, "type");
    name = jsonString(o, "name");
    enabled = boost::json::get_bool(o, "enabled", true);
    if (auto* created = o.if_contains("createdAt")) {
        createdAt = parseOptionalTime(*created);
    }
    if (auto* expires = o.if_contains("expiresAt")) {
        expiresAt = parseOptionalTime(*expires);
    }
    data = jsonObjectOrEmpty(o, "data");
    attributes = jsonObjectOrEmpty(o, "attributes");
}

void UserAuthKey::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) {
    o["id"] = id;
    o["type"] = type;
    if (!name.empty()) {
        o["name"] = name;
    }
    o["enabled"] = enabled;
    writeOptionalTime(o, "createdAt", createdAt);
    writeOptionalTime(o, "expiresAt", expiresAt);
    if (!data.empty()) {
        o["data"] = data;
    }
    if (!attributes.empty()) {
        o["attributes"] = attributes;
    }
}

void UserRecord::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    if (auto* profileVal = o.if_contains("profile")) {
        if (profileVal->is_object()) {
            profile.jsonIn(profileVal->as_object(), opts);
        }
    }
    roles.clear();
    if (auto* rolesVal = o.if_contains("roles")) {
        if (rolesVal->is_array()) {
            for (const auto& item : rolesVal->as_array()) {
                if (item.is_string()) {
                    const std::string roleName(item.as_string().c_str());
                    if (!roleName.empty() &&
                        std::find(roles.begin(), roles.end(), roleName) == roles.end()) {
                        roles.push_back(roleName);
                    }
                } else if (item.is_object()) {
                    IdentityRef ref;
                    ref.jsonIn(item.as_object(), opts);
                    const std::string roleName = ref.name.empty() ? ref.type : ref.name;
                    if (!roleName.empty() &&
                        std::find(roles.begin(), roles.end(), roleName) == roles.end()) {
                        roles.push_back(roleName);
                    }
                }
            }
        }
    }
    keys.clear();
    if (auto* keysVal = o.if_contains("keys")) {
        if (keysVal->is_array()) {
            for (const auto& item : keysVal->as_array()) {
                if (!item.is_object()) {
                    continue;
                }
                UserAuthKey key;
                key.jsonIn(item.as_object(), opts);
                keys.push_back(std::move(key));
            }
        }
    }
    attributes = jsonObjectOrEmpty(o, "attributes");
}

void UserRecord::jsonOut(boost::json::object& o, const JsonFormOptions& opts) {
    boost::json::object profileObj;
    profile.jsonOut(profileObj, opts);
    o["profile"] = profileObj;

    boost::json::array rolesArray;
    for (const auto& role : roles) {
        rolesArray.push_back(boost::json::value(role));
    }
    o["roles"] = rolesArray;

    boost::json::array keysArray;
    for (auto& key : keys) {
        boost::json::object keyObj;
        key.jsonOut(keyObj, opts);
        keysArray.push_back(keyObj);
    }
    o["keys"] = keysArray;

    if (!attributes.empty()) {
        o["attributes"] = attributes;
    }
}


UserRecord& DefaultUserStore::requireUser(const std::string& userName) {
    const auto it = m_users.find(userName);
    if (it == m_users.end()) {
        throw std::out_of_range("user not found: " + userName);
    }
    return it->second;
}

const UserRecord& DefaultUserStore::requireUser(const std::string& userName) const {
    const auto it = m_users.find(userName);
    if (it == m_users.end()) {
        throw std::out_of_range("user not found: " + userName);
    }
    return it->second;
}

bool DefaultUserStore::hasUser(const std::string& userName) const {
    return m_users.find(userName) != m_users.end();
}

std::optional<UserProfile> DefaultUserStore::getUserProfile(const std::string& userName) const {
    const auto it = m_users.find(userName);
    if (it == m_users.end()) {
        return std::nullopt;
    }
    return it->second.profile;
}

std::optional<UserRecord> DefaultUserStore::getUserRecord(const std::string& userName) const {
    const auto it = m_users.find(userName);
    if (it == m_users.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string> DefaultUserStore::listUsers() const {
    std::vector<std::string> names;
    names.reserve(m_users.size());
    for (const auto& [name, record] : m_users) {
        (void)record;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void DefaultUserStore::addUser(const UserRecord& user) {
    if (user.profile.name.empty()) {
        throw std::invalid_argument("UserRecord.profile.name is required");
    }
    m_users[user.profile.name] = user;
    m_users[user.profile.name].profile.name = user.profile.name;
}

void DefaultUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    auto& record = requireUser(userName);
    record.profile = profile;
    record.profile.name = userName;
}

void DefaultUserStore::removeUser(const std::string& userName) { m_users.erase(userName); }

void DefaultUserStore::clear() { m_users.clear(); }

void DefaultUserStore::enableUser(const std::string& userName) {
    requireUser(userName).profile.enabled = true;
}

void DefaultUserStore::disableUser(const std::string& userName) {
    requireUser(userName).profile.enabled = false;
}

std::vector<std::string> DefaultUserStore::getRoles(const std::string& userName) const {
    return requireUser(userName).roles;
}

void DefaultUserStore::setRoles(const std::string& userName, const std::vector<std::string>& roles) {
    requireUser(userName).roles = roles;
}

void DefaultUserStore::addRole(const std::string& userName, const std::string& roleName) {
    auto& roles = requireUser(userName).roles;
    if (std::find(roles.begin(), roles.end(), roleName) == roles.end()) {
        roles.push_back(roleName);
    }
}

void DefaultUserStore::removeRole(const std::string& userName, const std::string& roleName) {
    auto& roles = requireUser(userName).roles;
    roles.erase(std::remove(roles.begin(), roles.end(), roleName), roles.end());
}

std::vector<UserAuthKey> DefaultUserStore::getKeys(const std::string& userName) const {
    return requireUser(userName).keys;
}

std::vector<UserAuthKey> DefaultUserStore::getKeysByType(const std::string& userName,
                                                         const std::string& keyType) const {
    std::vector<UserAuthKey> found;
    for (const auto& key : requireUser(userName).keys) {
        if (key.type == keyType) {
            found.push_back(key);
        }
    }
    return found;
}

std::optional<UserAuthKey> DefaultUserStore::getKey(const std::string& userName,
                                                    const std::string& keyId) const {
    for (const auto& key : requireUser(userName).keys) {
        if (key.id == keyId) {
            return key;
        }
    }
    return std::nullopt;
}

void DefaultUserStore::addKey(const std::string& userName, const UserAuthKey& key) {
    requireUser(userName).keys.push_back(key);
}

void DefaultUserStore::updateKey(const std::string& userName, const UserAuthKey& key) {
    auto& keys = requireUser(userName).keys;
    for (auto& existing : keys) {
        if (existing.id == key.id) {
            existing = key;
            return;
        }
    }
    keys.push_back(key);
}

void DefaultUserStore::removeKey(const std::string& userName, const std::string& keyId) {
    auto& keys = requireUser(userName).keys;
    keys.erase(std::remove_if(keys.begin(), keys.end(),
                              [&](const UserAuthKey& k) { return k.id == keyId; }),
               keys.end());
}

void DefaultUserStore::enableKey(const std::string& userName, const std::string& keyId) {
    for (auto& key : requireUser(userName).keys) {
        if (key.id == keyId) {
            key.enabled = true;
            return;
        }
    }
}

void DefaultUserStore::disableKey(const std::string& userName, const std::string& keyId) {
    for (auto& key : requireUser(userName).keys) {
        if (key.id == keyId) {
            key.enabled = false;
            return;
        }
    }
}

void DefaultUserStore::jsonInUsersArray(const boost::json::array& users, const JsonFormOptions& opts) {
    for (const auto& item : users) {
        if (!item.is_object()) {
            continue;
        }
        UserRecord record;
        record.jsonIn(item.as_object(), opts);
        if (record.profile.name.empty()) {
            continue;
        }
        addUser(record);
    }
}

void DefaultUserStore::jsonIn(const boost::json::value& in, const JsonFormOptions& opts) {
    if (in.is_null()) {
        return;
    }
    if (in.is_array()) {
        clear();
        jsonInUsersArray(in.as_array(), opts);
        return;
    }
    if (in.is_object()) {
        jsonIn(in.as_object(), opts);
    }
}

boost::json::value DefaultUserStore::jsonOut(const JsonFormOptions& opts) {
    boost::json::object envelope;
    jsonOut(envelope, opts);
    return envelope;
}

void DefaultUserStore::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    clear();
    if (auto* users = o.if_contains("users")) {
        if (users->is_array()) {
            jsonInUsersArray(users->as_array(), opts);
        }
    }
}

void DefaultUserStore::jsonOut(boost::json::object& out, const JsonFormOptions& opts) {
    out["version"] = 1;
    boost::json::array users;
    for (const auto& [name, record] : m_users) {
        (void)name;
        boost::json::object item;
        UserRecord copy = record;
        copy.jsonOut(item, opts);
        users.push_back(item);
    }
    out["users"] = users;
}

bool DecoratedUserStore::hasUser(const std::string& userName) const {
    return m_wrapped->hasUser(userName);
}

std::optional<UserProfile> DecoratedUserStore::getUserProfile(const std::string& userName) const {
    return m_wrapped->getUserProfile(userName);
}

std::optional<UserRecord> DecoratedUserStore::getUserRecord(const std::string& userName) const {
    return m_wrapped->getUserRecord(userName);
}

std::vector<std::string> DecoratedUserStore::listUsers() const { return m_wrapped->listUsers(); }

void DecoratedUserStore::addUser(const UserRecord& user) { m_wrapped->addUser(user); }

void DecoratedUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    m_wrapped->updateUserProfile(userName, profile);
}

void DecoratedUserStore::removeUser(const std::string& userName) { m_wrapped->removeUser(userName); }

void DecoratedUserStore::clear() { m_wrapped->clear(); }

void DecoratedUserStore::enableUser(const std::string& userName) { m_wrapped->enableUser(userName); }

void DecoratedUserStore::disableUser(const std::string& userName) { m_wrapped->disableUser(userName); }

std::vector<std::string> DecoratedUserStore::getRoles(const std::string& userName) const {
    return m_wrapped->getRoles(userName);
}

void DecoratedUserStore::setRoles(const std::string& userName, const std::vector<std::string>& roles) {
    m_wrapped->setRoles(userName, roles);
}

void DecoratedUserStore::addRole(const std::string& userName, const std::string& roleName) {
    m_wrapped->addRole(userName, roleName);
}

void DecoratedUserStore::removeRole(const std::string& userName, const std::string& roleName) {
    m_wrapped->removeRole(userName, roleName);
}

std::vector<UserAuthKey> DecoratedUserStore::getKeys(const std::string& userName) const {
    return m_wrapped->getKeys(userName);
}

std::vector<UserAuthKey> DecoratedUserStore::getKeysByType(const std::string& userName,
                                                           const std::string& keyType) const {
    return m_wrapped->getKeysByType(userName, keyType);
}

std::optional<UserAuthKey> DecoratedUserStore::getKey(const std::string& userName,
                                                      const std::string& keyId) const {
    return m_wrapped->getKey(userName, keyId);
}

void DecoratedUserStore::addKey(const std::string& userName, const UserAuthKey& key) {
    m_wrapped->addKey(userName, key);
}

void DecoratedUserStore::updateKey(const std::string& userName, const UserAuthKey& key) {
    m_wrapped->updateKey(userName, key);
}

void DecoratedUserStore::removeKey(const std::string& userName, const std::string& keyId) {
    m_wrapped->removeKey(userName, keyId);
}

void DecoratedUserStore::enableKey(const std::string& userName, const std::string& keyId) {
    m_wrapped->enableKey(userName, keyId);
}

void DecoratedUserStore::disableKey(const std::string& userName, const std::string& keyId) {
    m_wrapped->disableKey(userName, keyId);
}

void DecoratedUserStore::jsonIn(const boost::json::value& in, const JsonFormOptions& opts) {
    m_wrapped->jsonIn(in, opts);
}

boost::json::value DecoratedUserStore::jsonOut(const JsonFormOptions& opts) {
    return m_wrapped->jsonOut(opts);
}

void DecoratedUserStore::jsonIn(const boost::json::object& in, const JsonFormOptions& opts) {
    m_wrapped->jsonIn(in, opts);
}

void DecoratedUserStore::jsonOut(boost::json::object& out, const JsonFormOptions& opts) {
    m_wrapped->jsonOut(out, opts);
}

FileUserStore::FileUserStore(VolumeFile file, std::shared_ptr<UserStore> wrapped)
    : DecoratedUserStore(std::move(wrapped)), m_file(std::move(file)) {
    loadFromDisk();
}

FileUserStore::FileUserStore(std::filesystem::path path, std::shared_ptr<UserStore> wrapped)
    : DecoratedUserStore(std::move(wrapped)),
      m_ownedVolume(std::make_shared<LocalVolume>(
          path.parent_path().empty() ? "." : path.parent_path().string())),
      m_file(m_ownedVolume, path.filename().string()) {
    loadFromDisk();
}

void FileUserStore::loadFromDisk() {
    m_loadedCount = 0;
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

    wrapped()->jsonIn(root, JsonFormOptions::DEFAULT);
    m_loadedCount = wrapped()->listUsers().size();
}

void FileUserStore::writeStoreToDisk() {
    m_file.createParentDirectories();

    boost::json::object envelope;
    wrapped()->jsonOut(envelope, JsonFormOptions::DEFAULT);
    m_file.writeFileString(boost::json::serialize(envelope), "UTF-8");
}

void FileUserStore::flush() { writeStoreToDisk(); }

void FileUserStore::reload() { loadFromDisk(); }

bool FileUserStore::hasUser(const std::string& userName) const { return wrapped()->hasUser(userName); }

std::optional<UserProfile> FileUserStore::getUserProfile(const std::string& userName) const {
    return wrapped()->getUserProfile(userName);
}

std::optional<UserRecord> FileUserStore::getUserRecord(const std::string& userName) const {
    return wrapped()->getUserRecord(userName);
}

std::vector<std::string> FileUserStore::listUsers() const { return wrapped()->listUsers(); }

void FileUserStore::addUser(const UserRecord& user) {
    wrapped()->addUser(user);
    writeStoreToDisk();
}

void FileUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    wrapped()->updateUserProfile(userName, profile);
    writeStoreToDisk();
}

void FileUserStore::removeUser(const std::string& userName) {
    wrapped()->removeUser(userName);
    writeStoreToDisk();
}

void FileUserStore::clear() {
    wrapped()->clear();
    m_loadedCount = 0;
    writeStoreToDisk();
}

void FileUserStore::enableUser(const std::string& userName) {
    wrapped()->enableUser(userName);
    writeStoreToDisk();
}

void FileUserStore::disableUser(const std::string& userName) {
    wrapped()->disableUser(userName);
    writeStoreToDisk();
}

void FileUserStore::setRoles(const std::string& userName, const std::vector<std::string>& roles) {
    wrapped()->setRoles(userName, roles);
    writeStoreToDisk();
}

void FileUserStore::addRole(const std::string& userName, const std::string& roleName) {
    wrapped()->addRole(userName, roleName);
    writeStoreToDisk();
}

void FileUserStore::removeRole(const std::string& userName, const std::string& roleName) {
    wrapped()->removeRole(userName, roleName);
    writeStoreToDisk();
}

void FileUserStore::addKey(const std::string& userName, const UserAuthKey& key) {
    wrapped()->addKey(userName, key);
    writeStoreToDisk();
}

void FileUserStore::updateKey(const std::string& userName, const UserAuthKey& key) {
    wrapped()->updateKey(userName, key);
    writeStoreToDisk();
}

void FileUserStore::removeKey(const std::string& userName, const std::string& keyId) {
    wrapped()->removeKey(userName, keyId);
    writeStoreToDisk();
}

void FileUserStore::enableKey(const std::string& userName, const std::string& keyId) {
    wrapped()->enableKey(userName, keyId);
    writeStoreToDisk();
}

void FileUserStore::disableKey(const std::string& userName, const std::string& keyId) {
    wrapped()->disableKey(userName, keyId);
    writeStoreToDisk();
}

RegistryUserStore::RegistryUserStore(std::shared_ptr<::bas::reg::IRegistry> registry,
                                     std::string rrlPrefix, std::shared_ptr<UserStore> wrapped)
    : DecoratedUserStore(std::move(wrapped)), m_registry(std::move(registry)),
      m_prefix(normalizeRrlPrefix(std::move(rrlPrefix))) {
    if (!m_registry) {
        throw std::invalid_argument("RegistryUserStore requires a registry");
    }
    m_containers = dynamic_cast<::bas::reg::IContainerManager*>(m_registry.get());
    loadFromRegistry();
}

std::string RegistryUserStore::userPath(const std::string& userName) const {
    return joinRrlPath(m_prefix, userName);
}

void RegistryUserStore::loadFromRegistry() {
    m_loadedCount = 0;
    wrapped()->clear();
    if (m_prefix.empty()) {
        return;
    }

    const auto children = m_registry->list(m_prefix);
    for (const auto& [key, info] : children) {
        (void)info;
        const std::string userName = ::bas::reg::keyToString(key);
        if (userName.empty()) {
            continue;
        }
        const auto json = loadRegistryJson(*m_registry, m_containers, userPath(userName));
        if (!json.has_value()) {
            continue;
        }
        const auto record = userRecordFromRegistryJson(*json);
        if (!record.has_value()) {
            continue;
        }
        wrapped()->addUser(*record);
        ++m_loadedCount;
    }
}

void RegistryUserStore::flush() {
    if (m_containers != nullptr) {
        m_containers->flushCachedContainers();
    }
    m_registry->save();
}

void RegistryUserStore::writeUser(const std::string& userName) {
    const auto record = wrapped()->getUserRecord(userName);
    if (!record.has_value()) {
        return;
    }
    boost::json::object obj;
    UserRecord copy = *record;
    copy.jsonOut(obj, JsonFormOptions::DEFAULT);
    if (!saveRegistryJson(*m_registry, m_containers, userPath(userName), boost::json::value(obj))) {
        throw std::runtime_error("failed to write user to registry: " + userPath(userName));
    }
}

void RegistryUserStore::removeUserFromRegistry(const std::string& userName) {
    removeRegistryJson(*m_registry, m_containers, userPath(userName));
}

bool RegistryUserStore::hasUser(const std::string& userName) const {
    return wrapped()->hasUser(userName);
}

std::optional<UserProfile> RegistryUserStore::getUserProfile(const std::string& userName) const {
    return wrapped()->getUserProfile(userName);
}

std::optional<UserRecord> RegistryUserStore::getUserRecord(const std::string& userName) const {
    return wrapped()->getUserRecord(userName);
}

std::vector<std::string> RegistryUserStore::listUsers() const { return wrapped()->listUsers(); }

void RegistryUserStore::addUser(const UserRecord& user) {
    const bool isNew = !wrapped()->hasUser(user.profile.name);
    wrapped()->addUser(user);
    writeUser(user.profile.name);
    if (isNew) {
        ++m_loadedCount;
    }
    flush();
}

void RegistryUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    wrapped()->updateUserProfile(userName, profile);
    writeUser(userName);
    flush();
}

void RegistryUserStore::removeUser(const std::string& userName) {
    wrapped()->removeUser(userName);
    removeUserFromRegistry(userName);
    if (m_loadedCount > 0) {
        --m_loadedCount;
    }
    flush();
}

void RegistryUserStore::clear() {
    const auto users = wrapped()->listUsers();
    wrapped()->clear();
    for (const auto& userName : users) {
        removeUserFromRegistry(userName);
    }
    m_loadedCount = 0;
    flush();
}

void RegistryUserStore::enableUser(const std::string& userName) {
    wrapped()->enableUser(userName);
    writeUser(userName);
    flush();
}

void RegistryUserStore::disableUser(const std::string& userName) {
    wrapped()->disableUser(userName);
    writeUser(userName);
    flush();
}

void RegistryUserStore::setRoles(const std::string& userName, const std::vector<std::string>& roles) {
    wrapped()->setRoles(userName, roles);
    writeUser(userName);
    flush();
}

void RegistryUserStore::addRole(const std::string& userName, const std::string& roleName) {
    wrapped()->addRole(userName, roleName);
    writeUser(userName);
    flush();
}

void RegistryUserStore::removeRole(const std::string& userName, const std::string& roleName) {
    wrapped()->removeRole(userName, roleName);
    writeUser(userName);
    flush();
}

void RegistryUserStore::addKey(const std::string& userName, const UserAuthKey& key) {
    wrapped()->addKey(userName, key);
    writeUser(userName);
    flush();
}

void RegistryUserStore::updateKey(const std::string& userName, const UserAuthKey& key) {
    wrapped()->updateKey(userName, key);
    writeUser(userName);
    flush();
}

void RegistryUserStore::removeKey(const std::string& userName, const std::string& keyId) {
    wrapped()->removeKey(userName, keyId);
    writeUser(userName);
    flush();
}

void RegistryUserStore::enableKey(const std::string& userName, const std::string& keyId) {
    wrapped()->enableKey(userName, keyId);
    writeUser(userName);
    flush();
}

void RegistryUserStore::disableKey(const std::string& userName, const std::string& keyId) {
    wrapped()->disableKey(userName, keyId);
    writeUser(userName);
    flush();
}

bool verifyUserPassword(const UserStore& store, const std::string& userName,
                        const std::string& password) {
    const auto record = store.getUserRecord(userName);
    if (!record.has_value() || !record->profile.enabled) {
        return false;
    }
    const auto now = std::chrono::system_clock::now();
    for (const auto& key : record->keys) {
        if (passwordMatchesAuthKey(password, key, now)) {
            return true;
        }
    }
    return false;
}

} // namespace bas::security
