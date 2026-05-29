#include "user_store.hpp"

#include "bas/script/json.hpp"
#include "json.hpp"
#include "realm.hpp"

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

bool passwordMatchesKey(const std::string& password, const UserAuthKey& key,
                        std::chrono::system_clock::time_point now) {
    if (!key.enabled || key.type != "password-hash" || key.isExpired(now)) {
        return false;
    }
    if (auto* hash = key.data.if_contains("hash")) {
        if (hash->is_string()) {
            return hash->as_string() == password;
        }
    }
    return false;
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
                    IdentityRef ref;
                    ref.type = "role";
                    ref.name = std::string(item.as_string().c_str());
                    roles.push_back(std::move(ref));
                } else if (item.is_object()) {
                    IdentityRef ref;
                    identityRefJsonIn(ref, item.as_object());
                    if (ref.type.empty()) {
                        ref.type = "role";
                    }
                    roles.push_back(std::move(ref));
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
        boost::json::object roleObj;
        identityRefJsonOut(role, roleObj);
        rolesArray.push_back(roleObj);
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


Realm DefaultUserStore::storedUserRealm(const std::optional<Realm>& realm) {
    Realm r = realm.value_or(Realm::GLOBAL);
    if (r.type.empty()) {
        r.type = "global";
    }
    return r;
}

std::string DefaultUserStore::storageKey(const std::string& userName,
                                         const std::optional<Realm>& realm) const {
    return realmStorageKey(storedUserRealm(realm)) + '\x1f' + userName;
}

bool DefaultUserStore::realmFilterMatch(const Realm& stored,
                                        const std::optional<Realm>& filter) const {
    if (!filter.has_value()) {
        return true;
    }
    return realmMatches(stored, *filter);
}

DefaultUserStore::StoredUser& DefaultUserStore::requireUser(const std::string& userName,
                                                            const std::optional<Realm>& realm) {
    const auto it = m_users.find(storageKey(userName, realm));
    if (it == m_users.end()) {
        throw std::out_of_range("user not found: " + userName);
    }
    return it->second;
}

const DefaultUserStore::StoredUser& DefaultUserStore::requireUser(
    const std::string& userName, const std::optional<Realm>& realm) const {
    const auto it = m_users.find(storageKey(userName, realm));
    if (it == m_users.end()) {
        throw std::out_of_range("user not found: " + userName);
    }
    return it->second;
}

std::vector<Realm> DefaultUserStore::getRealms() const {
    std::vector<Realm> realms;
    for (const auto& [key, stored] : m_users) {
        (void)key;
        bool found = false;
        const Realm realm = storedUserRealm(stored.realm);
        for (const auto& r : realms) {
            if (realmScopedEqual(r, realm)) {
                found = true;
                break;
            }
        }
        if (!found) {
            realms.push_back(realm);
        }
    }
    return realms;
}

bool DefaultUserStore::hasUser(const std::string& userName) const {
    return m_users.find(storageKey(userName, std::nullopt)) != m_users.end();
}

std::optional<UserProfile> DefaultUserStore::getUserProfile(const std::string& userName) const {
    const auto it = m_users.find(storageKey(userName, std::nullopt));
    if (it == m_users.end()) {
        return std::nullopt;
    }
    return it->second.record.profile;
}

std::optional<UserRecord> DefaultUserStore::getUserRecord(const std::string& userName,
                                                          const std::optional<Realm>& realm) const {
    const auto it = m_users.find(storageKey(userName, realm));
    if (it == m_users.end()) {
        return std::nullopt;
    }
    return it->second.record;
}

std::vector<std::string> DefaultUserStore::listUsers(const std::optional<Realm>& realmFilter) const {
    std::vector<std::string> names;
    for (const auto& [key, stored] : m_users) {
        (void)key;
        if (realmFilterMatch(storedUserRealm(stored.realm), realmFilter)) {
            names.push_back(stored.name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

void DefaultUserStore::addUser(const UserRecord& user) {
    if (user.profile.name.empty()) {
        throw std::invalid_argument("UserRecord.profile.name is required");
    }
    StoredUser stored;
    stored.realm = std::nullopt;
    stored.name = user.profile.name;
    stored.record = user;
    m_users[storageKey(stored.name, stored.realm)] = std::move(stored);
}

void DefaultUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    auto& stored = requireUser(userName);
    stored.record.profile = profile;
    stored.record.profile.name = userName;
}

void DefaultUserStore::removeUser(const std::string& userName) {
    m_users.erase(storageKey(userName, std::nullopt));
}

void DefaultUserStore::clear(const std::optional<Realm>& realmFilter) {
    if (!realmFilter.has_value()) {
        m_users.clear();
        return;
    }
    for (auto it = m_users.begin(); it != m_users.end();) {
        if (realmFilterMatch(storedUserRealm(it->second.realm), realmFilter)) {
            it = m_users.erase(it);
        } else {
            ++it;
        }
    }
}

void DefaultUserStore::enableUser(const std::string& userName) {
    requireUser(userName).record.profile.enabled = true;
}

void DefaultUserStore::disableUser(const std::string& userName) {
    requireUser(userName).record.profile.enabled = false;
}

std::vector<IdentityRef> DefaultUserStore::getRoles(const std::string& userName) const {
    return requireUser(userName).record.roles;
}

void DefaultUserStore::setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) {
    requireUser(userName).record.roles = roles;
}

void DefaultUserStore::addRole(const std::string& userName, const IdentityRef& role) {
    auto& roles = requireUser(userName).record.roles;
    if (std::find(roles.begin(), roles.end(), role) == roles.end()) {
        roles.push_back(role);
    }
}

void DefaultUserStore::removeRole(const std::string& userName, const IdentityRef& role) {
    auto& roles = requireUser(userName).record.roles;
    roles.erase(std::remove(roles.begin(), roles.end(), role), roles.end());
}

std::vector<UserAuthKey> DefaultUserStore::getKeys(const std::string& userName) const {
    return requireUser(userName).record.keys;
}

std::vector<UserAuthKey> DefaultUserStore::getKeysByType(const std::string& userName,
                                                         const std::string& keyType) const {
    std::vector<UserAuthKey> found;
    for (const auto& key : requireUser(userName).record.keys) {
        if (key.type == keyType) {
            found.push_back(key);
        }
    }
    return found;
}

std::optional<UserAuthKey> DefaultUserStore::getKey(const std::string& userName,
                                                    const std::string& keyId) const {
    for (const auto& key : requireUser(userName).record.keys) {
        if (key.id == keyId) {
            return key;
        }
    }
    return std::nullopt;
}

void DefaultUserStore::addKey(const std::string& userName, const UserAuthKey& key) {
    requireUser(userName).record.keys.push_back(key);
}

void DefaultUserStore::updateKey(const std::string& userName, const UserAuthKey& key) {
    auto& keys = requireUser(userName).record.keys;
    for (auto& existing : keys) {
        if (existing.id == key.id) {
            existing = key;
            return;
        }
    }
    keys.push_back(key);
}

void DefaultUserStore::removeKey(const std::string& userName, const std::string& keyId) {
    auto& keys = requireUser(userName).record.keys;
    keys.erase(std::remove_if(keys.begin(), keys.end(),
                              [&](const UserAuthKey& k) { return k.id == keyId; }),
               keys.end());
}

void DefaultUserStore::enableKey(const std::string& userName, const std::string& keyId) {
    for (auto& key : requireUser(userName).record.keys) {
        if (key.id == keyId) {
            key.enabled = true;
            return;
        }
    }
}

void DefaultUserStore::disableKey(const std::string& userName, const std::string& keyId) {
    for (auto& key : requireUser(userName).record.keys) {
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
        clear(std::nullopt);
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
    clear(std::nullopt);
    if (auto* users = o.if_contains("users")) {
        if (users->is_array()) {
            jsonInUsersArray(users->as_array(), opts);
        }
    }
}

void DefaultUserStore::jsonOut(boost::json::object& out, const JsonFormOptions& opts) {
    out["version"] = 1;
    boost::json::array users;
    for (const auto& [key, stored] : m_users) {
        (void)key;
        boost::json::object item;
        UserRecord copy = stored.record;
        copy.jsonOut(item, opts);
        users.push_back(item);
    }
    out["users"] = users;
}

DecoratedUserStore::DecoratedUserStore(VariantPtr<UserStore> wrapped)
    : m_wrapped(std::move(wrapped)) {}

std::vector<Realm> DecoratedUserStore::getRealms() const { return m_wrapped->getRealms(); }

bool DecoratedUserStore::hasUser(const std::string& userName) const {
    return m_wrapped->hasUser(userName);
}

std::optional<UserProfile> DecoratedUserStore::getUserProfile(const std::string& userName) const {
    return m_wrapped->getUserProfile(userName);
}

std::optional<UserRecord> DecoratedUserStore::getUserRecord(const std::string& userName,
                                                            const std::optional<Realm>& realm) const {
    return m_wrapped->getUserRecord(userName, realm);
}

std::vector<std::string> DecoratedUserStore::listUsers(const std::optional<Realm>& realmFilter) const {
    return m_wrapped->listUsers(realmFilter);
}

void DecoratedUserStore::addUser(const UserRecord& user) { m_wrapped->addUser(user); }

void DecoratedUserStore::updateUserProfile(const std::string& userName, const UserProfile& profile) {
    m_wrapped->updateUserProfile(userName, profile);
}

void DecoratedUserStore::removeUser(const std::string& userName) { m_wrapped->removeUser(userName); }

void DecoratedUserStore::clear(const std::optional<Realm>& realmFilter) {
    m_wrapped->clear(realmFilter);
}

void DecoratedUserStore::enableUser(const std::string& userName) { m_wrapped->enableUser(userName); }

void DecoratedUserStore::disableUser(const std::string& userName) { m_wrapped->disableUser(userName); }

std::vector<IdentityRef> DecoratedUserStore::getRoles(const std::string& userName) const {
    return m_wrapped->getRoles(userName);
}

void DecoratedUserStore::setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) {
    m_wrapped->setRoles(userName, roles);
}

void DecoratedUserStore::addRole(const std::string& userName, const IdentityRef& role) {
    m_wrapped->addRole(userName, role);
}

void DecoratedUserStore::removeRole(const std::string& userName, const IdentityRef& role) {
    m_wrapped->removeRole(userName, role);
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

FileUserStore::FileUserStore(VolumeFile file, VariantPtr<UserStore> wrapped)
    : DecoratedUserStore(std::move(wrapped)), m_file(std::move(file)) {
    loadFromDisk();
}

FileUserStore::FileUserStore(std::filesystem::path path, VariantPtr<UserStore> wrapped)
    : DecoratedUserStore(std::move(wrapped)),
      m_ownedVolume(std::make_unique<LocalVolume>(
          path.parent_path().empty() ? "." : path.parent_path().string())),
      m_file(m_ownedVolume.get(), path.filename().string()) {
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

std::vector<Realm> FileUserStore::getRealms() const { return wrapped()->getRealms(); }

bool FileUserStore::hasUser(const std::string& userName) const { return wrapped()->hasUser(userName); }

std::optional<UserProfile> FileUserStore::getUserProfile(const std::string& userName) const {
    return wrapped()->getUserProfile(userName);
}

std::optional<UserRecord> FileUserStore::getUserRecord(const std::string& userName,
                                                       const std::optional<Realm>& realm) const {
    return wrapped()->getUserRecord(userName, realm);
}

std::vector<std::string> FileUserStore::listUsers(const std::optional<Realm>& realmFilter) const {
    return wrapped()->listUsers(realmFilter);
}

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

void FileUserStore::clear(const std::optional<Realm>& realmFilter) {
    wrapped()->clear(realmFilter);
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

void FileUserStore::setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) {
    wrapped()->setRoles(userName, roles);
    writeStoreToDisk();
}

void FileUserStore::addRole(const std::string& userName, const IdentityRef& role) {
    wrapped()->addRole(userName, role);
    writeStoreToDisk();
}

void FileUserStore::removeRole(const std::string& userName, const IdentityRef& role) {
    wrapped()->removeRole(userName, role);
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

bool verifyUserPassword(const UserStore& store, const std::string& userName,
                        const std::string& password, const std::optional<Realm>& realm) {
    const auto record = store.getUserRecord(userName, realm);
    if (!record.has_value() || !record->profile.enabled) {
        return false;
    }
    const auto now = std::chrono::system_clock::now();
    for (const auto& key : record->keys) {
        if (passwordMatchesKey(password, key, now)) {
            return true;
        }
    }
    return false;
}

void populateDemoUsers(DefaultUserStore& store) {
    auto makePasswordKey = [](const std::string& id, const std::string& passwordHash) {
        UserAuthKey key;
        key.id = id;
        key.type = "password-hash";
        key.name = "Main Password";
        key.enabled = true;
        key.createdAt = std::chrono::system_clock::now();
        key.data["algorithm"] = boost::json::value("demo-plain");
        key.data["hash"] = boost::json::value(passwordHash);
        return key;
    };

    {
        UserRecord alice;
        alice.profile.name = "alice";
        alice.profile.displayName = "Alice";
        alice.profile.email = "alice@example.com";
        alice.profile.attributes["department"] = boost::json::value("factory");
        alice.roles = {roleRef(Realm::GLOBAL, "operator")};
        alice.keys.push_back(makePasswordKey("pwd-main", "alice"));
        store.addUser(alice);
    }

    {
        UserRecord bob;
        bob.profile.name = "bob";
        bob.profile.displayName = "Bob";
        bob.roles = {roleRef(Realm::GLOBAL, "operator")};
        bob.keys.push_back(makePasswordKey("pwd-main", "bob"));
        store.addUser(bob);
    }

    {
        UserRecord jUser;
        jUser.profile.name = "j";
        jUser.profile.displayName = "J";
        jUser.roles = {roleRef(Realm::GLOBAL, "operator")};
        jUser.keys.push_back(makePasswordKey("pwd-main", "k"));
        store.addUser(jUser);
    }

    {
        UserRecord admin;
        admin.profile.name = "admin";
        admin.profile.displayName = "Administrator";
        admin.profile.email = "admin@example.com";
        admin.roles = {roleRef(Realm::GLOBAL, "admin"), roleRef(Realm::GLOBAL, "operator")};
        admin.keys.push_back(makePasswordKey("pwd-main", "admin"));
        store.addUser(admin);
    }
}

} // namespace bas::security
