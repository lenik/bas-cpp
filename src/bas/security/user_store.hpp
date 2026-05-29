#ifndef BAS_SECURITY_USER_STORE_HPP
#define BAS_SECURITY_USER_STORE_HPP

#include "../fmt/JsonForm.hpp"
#include "../util/ptr.hpp"
#include "../volume/LocalVolume.hpp"
#include "../volume/VolumeFile.hpp"
#include "command_support.hpp"
#include "identity.hpp"
#include "realm.hpp"
#include "types.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bas::security {

/** Non-authentication user profile (no secrets, hashes, or tokens). */
struct UserProfile : IJsonForm {
    std::string name;
    std::string displayName;
    std::string email;

    bool enabled = true;

    JsonObject attributes;

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;
};

/**
 * Server-side verification material (password-hash, public-key, fingerprint, …).
 * Not a login Credential — must not hold plaintext secrets.
 */
struct UserAuthKey : IJsonForm {
    std::string id;
    std::string type;
    std::string name;

    bool enabled = true;

    std::optional<std::chrono::system_clock::time_point> createdAt;
    std::optional<std::chrono::system_clock::time_point> expiresAt;

    JsonObject data;
    JsonObject attributes;

    bool isExpired(std::chrono::system_clock::time_point now) const {
        return expiresAt.has_value() && now >= *expiresAt;
    }

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;
};

/** Aggregate: profile + role identities + auth keys (+ record-level metadata). */
struct UserRecord : IJsonForm {
    UserProfile profile;
    std::vector<IdentityRef> roles;
    std::vector<UserAuthKey> keys;

    JsonObject attributes;

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;
};

class UserStore : public IJsonForm, public ICommandSupport {
  public:
    virtual ~UserStore() = default;

    /** Human-readable store kind (e.g. demo, or file path). */
    virtual std::string storeLabel() const { return "memory"; }

    virtual std::filesystem::path storePath() const { return {}; }

    virtual bool canReloadFromDisk() const { return false; }

    virtual void reloadFromDisk() {}

    virtual bool canSaveToDisk() const { return canReloadFromDisk(); }

    virtual void saveToDisk() {}

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    virtual std::vector<Realm> getRealms() const = 0;

    virtual bool hasUser(const std::string& userName) const = 0;

    virtual std::optional<UserProfile> getUserProfile(const std::string& userName) const = 0;

    virtual std::optional<UserRecord> getUserRecord(
        const std::string& userName,
        const std::optional<Realm>& realm = std::nullopt) const = 0;

    virtual std::vector<std::string> listUsers(
        const std::optional<Realm>& realmFilter = std::nullopt) const = 0;

    virtual void addUser(const UserRecord& user) = 0;

    virtual void updateUserProfile(const std::string& userName, const UserProfile& profile) = 0;

    virtual void removeUser(const std::string& userName) = 0;

    virtual void clear(const std::optional<Realm>& realmFilter = std::nullopt) = 0;

    virtual void enableUser(const std::string& userName) = 0;
    virtual void disableUser(const std::string& userName) = 0;

    virtual std::vector<IdentityRef> getRoles(const std::string& userName) const = 0;

    virtual void setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) = 0;

    virtual void addRole(const std::string& userName, const IdentityRef& role) = 0;

    virtual void removeRole(const std::string& userName, const IdentityRef& role) = 0;

    virtual std::vector<UserAuthKey> getKeys(const std::string& userName) const = 0;

    virtual std::vector<UserAuthKey> getKeysByType(const std::string& userName,
                                                   const std::string& keyType) const = 0;

    virtual std::optional<UserAuthKey> getKey(const std::string& userName,
                                              const std::string& keyId) const = 0;

    virtual void addKey(const std::string& userName, const UserAuthKey& key) = 0;

    virtual void updateKey(const std::string& userName, const UserAuthKey& key) = 0;

    virtual void removeKey(const std::string& userName, const std::string& keyId) = 0;

    virtual void enableKey(const std::string& userName, const std::string& keyId) = 0;

    virtual void disableKey(const std::string& userName, const std::string& keyId) = 0;

  protected:
    virtual int invokeUser(std::vector<std::string>& args);
};

class DefaultUserStore : public UserStore {
  public:
    std::vector<Realm> getRealms() const override;

    bool hasUser(const std::string& userName) const override;
    std::optional<UserProfile> getUserProfile(const std::string& userName) const override;
    std::optional<UserRecord> getUserRecord(const std::string& userName,
                                            const std::optional<Realm>& realm =
                                                std::nullopt) const override;
    std::vector<std::string> listUsers(const std::optional<Realm>& realmFilter) const override;
    void addUser(const UserRecord& user) override;
    void updateUserProfile(const std::string& userName, const UserProfile& profile) override;
    void removeUser(const std::string& userName) override;
    void clear(const std::optional<Realm>& realmFilter) override;
    void enableUser(const std::string& userName) override;
    void disableUser(const std::string& userName) override;
    std::vector<IdentityRef> getRoles(const std::string& userName) const override;
    void setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) override;
    void addRole(const std::string& userName, const IdentityRef& role) override;
    void removeRole(const std::string& userName, const IdentityRef& role) override;
    std::vector<UserAuthKey> getKeys(const std::string& userName) const override;
    std::vector<UserAuthKey> getKeysByType(const std::string& userName,
                                           const std::string& keyType) const override;
    std::optional<UserAuthKey> getKey(const std::string& userName,
                                      const std::string& keyId) const override;
    void addKey(const std::string& userName, const UserAuthKey& key) override;
    void updateKey(const std::string& userName, const UserAuthKey& key) override;
    void removeKey(const std::string& userName, const std::string& keyId) override;
    void enableKey(const std::string& userName, const std::string& keyId) override;
    void disableKey(const std::string& userName, const std::string& keyId) override;

    void jsonIn(const boost::json::value& in, const JsonFormOptions& opts) override;
    boost::json::value jsonOut(const JsonFormOptions& opts = JsonFormOptions::DEFAULT) override;
    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;

  private:
    struct StoredUser {
        std::optional<Realm> realm;
        std::string name;
        UserRecord record;
    };

    static Realm storedUserRealm(const std::optional<Realm>& realm);

    std::string storageKey(const std::string& userName, const std::optional<Realm>& realm) const;
    StoredUser& requireUser(const std::string& userName,
                            const std::optional<Realm>& realm = std::nullopt);
    const StoredUser& requireUser(const std::string& userName,
                                   const std::optional<Realm>& realm = std::nullopt) const;
    bool realmFilterMatch(const Realm& stored, const std::optional<Realm>& filter) const;

    void jsonInUsersArray(const boost::json::array& users, const JsonFormOptions& opts);

    std::unordered_map<std::string, StoredUser> m_users;
};

class DecoratedUserStore : public UserStore {
  public:
    explicit DecoratedUserStore(VariantPtr<UserStore> wrapped);

    std::vector<Realm> getRealms() const override;
    bool hasUser(const std::string& userName) const override;
    std::optional<UserProfile> getUserProfile(const std::string& userName) const override;
    std::optional<UserRecord> getUserRecord(const std::string& userName,
                                            const std::optional<Realm>& realm =
                                                std::nullopt) const override;
    std::vector<std::string> listUsers(const std::optional<Realm>& realmFilter) const override;
    void addUser(const UserRecord& user) override;
    void updateUserProfile(const std::string& userName, const UserProfile& profile) override;
    void removeUser(const std::string& userName) override;
    void clear(const std::optional<Realm>& realmFilter) override;
    void enableUser(const std::string& userName) override;
    void disableUser(const std::string& userName) override;
    std::vector<IdentityRef> getRoles(const std::string& userName) const override;
    void setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) override;
    void addRole(const std::string& userName, const IdentityRef& role) override;
    void removeRole(const std::string& userName, const IdentityRef& role) override;
    std::vector<UserAuthKey> getKeys(const std::string& userName) const override;
    std::vector<UserAuthKey> getKeysByType(const std::string& userName,
                                           const std::string& keyType) const override;
    std::optional<UserAuthKey> getKey(const std::string& userName,
                                      const std::string& keyId) const override;
    void addKey(const std::string& userName, const UserAuthKey& key) override;
    void updateKey(const std::string& userName, const UserAuthKey& key) override;
    void removeKey(const std::string& userName, const std::string& keyId) override;
    void enableKey(const std::string& userName, const std::string& keyId) override;
    void disableKey(const std::string& userName, const std::string& keyId) override;

    void jsonIn(const boost::json::value& in, const JsonFormOptions& opts) override;
    boost::json::value jsonOut(const JsonFormOptions& opts = JsonFormOptions::DEFAULT) override;
    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    const UserStore* wrapped() const { return m_wrapped.operator->(); }
    UserStore* wrapped() { return m_wrapped.operator->(); }

  protected:
    VariantPtr<UserStore> m_wrapped;
};

/** Persists users to a JSON file (demo only: password hashes in plain text). */
class FileUserStore : public DecoratedUserStore {
  public:
    std::vector<Realm> getRealms() const override;
    explicit FileUserStore(VolumeFile file, VariantPtr<UserStore> wrapped);

    /** Convenience: owns a @ref LocalVolume rooted at @a path's parent directory. */
    explicit FileUserStore(std::filesystem::path path, VariantPtr<UserStore> wrapped);

    bool hasUser(const std::string& userName) const override;
    std::optional<UserProfile> getUserProfile(const std::string& userName) const override;
    std::optional<UserRecord> getUserRecord(const std::string& userName,
                                            const std::optional<Realm>& realm =
                                                std::nullopt) const override;
    std::vector<std::string> listUsers(const std::optional<Realm>& realmFilter) const override;
    void addUser(const UserRecord& user) override;
    void updateUserProfile(const std::string& userName, const UserProfile& profile) override;
    void removeUser(const std::string& userName) override;
    void clear(const std::optional<Realm>& realmFilter) override;
    void enableUser(const std::string& userName) override;
    void disableUser(const std::string& userName) override;
    void setRoles(const std::string& userName, const std::vector<IdentityRef>& roles) override;
    void addRole(const std::string& userName, const IdentityRef& role) override;
    void removeRole(const std::string& userName, const IdentityRef& role) override;
    void addKey(const std::string& userName, const UserAuthKey& key) override;
    void updateKey(const std::string& userName, const UserAuthKey& key) override;
    void removeKey(const std::string& userName, const std::string& keyId) override;
    void enableKey(const std::string& userName, const std::string& keyId) override;
    void disableKey(const std::string& userName, const std::string& keyId) override;

    const VolumeFile& file() const { return m_file; }
    std::size_t loadedCount() const { return m_loadedCount; }
    void flush();
    void reload();

    std::string storeLabel() const override;
    std::filesystem::path storePath() const override;
    bool canReloadFromDisk() const override;
    void reloadFromDisk() override;
    void saveToDisk() override;

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    void loadFromDisk();
    void writeStoreToDisk();

    std::unique_ptr<LocalVolume> m_ownedVolume;
    VolumeFile m_file;
    std::size_t m_loadedCount = 0;
};

/** Verify password credential against enabled password-hash keys (demo: plain hash in data). */
bool verifyUserPassword(const UserStore& store, const std::string& userName, const std::string& password,
                        const std::optional<Realm>& realm = std::nullopt);

/** Fill @a store with built-in sample users (alice, bob, j, admin). */
void populateDemoUsers(DefaultUserStore& store);

/** DefaultUserStore preloaded with @ref populateDemoUsers. */
class DemoUserStore : public DefaultUserStore {
  public:
    DemoUserStore() { populateDemoUsers(*this); }

    std::string storeLabel() const override;
};

} // namespace bas::security

#endif
