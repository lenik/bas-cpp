#ifndef BAS_SECURITY_CREDENTIAL_HPP
#define BAS_SECURITY_CREDENTIAL_HPP

#include "command_support.hpp"
#include "types.hpp"
#include "realm.hpp"

#include "../fmt/JsonForm.hpp"
#include "../util/ptr.hpp"

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace bas::security {

struct CredentialRef {
    std::string store;
    std::string id;

    bool empty() const { return store.empty() && id.empty(); }

    explicit operator bool() const { return !empty(); }

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts);
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) const;
};

struct CredentialMeta {
    std::string type;

    std::string name;
    std::string subjectHint;
    std::string serviceHint;
    Realm realm;

    std::optional<std::chrono::system_clock::time_point> expiresAt;

    JsonObject attributes;

    bool isExpired(std::chrono::system_clock::time_point now) const {
        return expiresAt.has_value() && now >= *expiresAt;
    }

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts);
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) const;
};

class SecretValue {
  public:
    SecretValue() = default;
    explicit SecretValue(std::string value);

    SecretValue(const SecretValue&) = delete;
    SecretValue& operator=(const SecretValue&) = delete;

    SecretValue(SecretValue&&) noexcept;
    SecretValue& operator=(SecretValue&&) noexcept;

    ~SecretValue();

    const std::string& value() const;
    std::string masked() const;

    void clear();

  private:
    std::string m_value;
};

struct Credential {
    CredentialRef ref;
    CredentialMeta meta;

    std::optional<SecretValue> secret;
    JsonObject publicData;

    Credential() = default;
    Credential(const Credential&) = delete;
    Credential& operator=(const Credential&) = delete;
    Credential(Credential&&) noexcept = default;
    Credential& operator=(Credential&&) noexcept = default;

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts);
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) const;
};

struct CredentialInfo {
    CredentialRef ref;
    CredentialMeta meta;
};

struct CredentialRequest {
    std::vector<std::string> types;

    std::string identityType;
    std::string subjectHint;
    std::string serviceHint;
    Realm realmHint;

    Permission permission;

    bool interactive = false;

    std::string reason;
    JsonObject context;

    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts);
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts);
};

class CredentialManager : public IJsonForm, public ICommandSupport {
  public:
    virtual ~CredentialManager() = default;

    virtual std::filesystem::path credentialPath() const { return {}; }

    virtual bool canReloadCredentials() const { return false; }

    virtual void reloadCredentials() {}

    virtual bool canSaveCredentials() const { return canReloadCredentials(); }

    virtual void saveCredentials() {}

    virtual std::size_t credentialPersistedCount() const { return 0; }

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    /**
     * Get a credential that matches the request.
     *
     * If there are multiple credentials that match the request, the one with the highest
     * priority is returned.
     *
     * If no credential matches the request, std::nullopt is returned.
     */
    virtual std::optional<Credential> get(const CredentialRequest& request) = 0;

    /**
     * Put a fully defined credential.

     If the credential has an empty id, a new id will be generated.
     * If a credential with the same id already exists, it will be replaced.
     */
    virtual CredentialRef put(Credential credential) = 0;

    /**
     * Load a credential by its reference.
     *
     * If the credential does not exist, std::nullopt is returned.
     */
    virtual std::optional<Credential> load(const CredentialRef& ref) = 0;

    /**
     * List credentials that match the request.
     *
     * If the request has an empty type, all credentials will be returned.
     */
    virtual std::vector<CredentialInfo> list(const CredentialRequest& request) const = 0;

    /**
     * Remove a credential by its reference.
     *
     * If the credential does not exist, nothing happens.
     */
    virtual void remove(const CredentialRef& ref) = 0;

    /**
     * Clear all credentials.
     */
    virtual void clear() = 0;
};

class DefaultCredentialManager : public CredentialManager {
  public:
    static constexpr const char* kStore = "memory";

    std::optional<Credential> get(const CredentialRequest& request) override;
    CredentialRef put(Credential credential) override;
    std::optional<Credential> load(const CredentialRef& ref) override;
    std::vector<CredentialInfo> list(const CredentialRequest& request) const override;
    void remove(const CredentialRef& ref) override;
    void clear() override;

    bool likeObject() override { return false; }
    void jsonIn(const boost::json::value& in, const JsonFormOptions& opts) override;
    boost::json::value jsonOut(const JsonFormOptions& opts = JsonFormOptions::DEFAULT) override;
    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;

  private:
    std::vector<Credential> m_credentials;
    std::size_t m_nextId = 1;
};

class DecoratedCredentialManager : public CredentialManager {
  public:
    explicit DecoratedCredentialManager(VariantPtr<CredentialManager> wrapped);

    std::optional<Credential> get(const CredentialRequest& request) override;
    CredentialRef put(Credential credential) override;
    std::optional<Credential> load(const CredentialRef& ref) override;
    std::vector<CredentialInfo> list(const CredentialRequest& request) const override;
    void remove(const CredentialRef& ref) override;
    void clear() override;

    bool likeObject() override { return wrapped()->likeObject(); }
    void jsonIn(const boost::json::value& in, const JsonFormOptions& opts) override;
    boost::json::value jsonOut(const JsonFormOptions& opts = JsonFormOptions::DEFAULT) override;
    void jsonIn(const boost::json::object& in, const JsonFormOptions& opts) override;
    void jsonOut(boost::json::object& out, const JsonFormOptions& opts) override;

    const CredentialManager* wrapped() const { return m_wrapped.operator->(); }
    CredentialManager* wrapped() { return m_wrapped.operator->(); }

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  protected:
    VariantPtr<CredentialManager> m_wrapped;
};

/** Persists credentials to a JSON file (demo only: secrets stored in plain text). */
class FileCredentialManager : public DecoratedCredentialManager {
  public:
    static constexpr const char* kStore = "file";

    explicit FileCredentialManager(std::filesystem::path path,
                                   VariantPtr<CredentialManager> wrapped);

    std::optional<Credential> get(const CredentialRequest& request) override;
    CredentialRef put(Credential credential) override;
    std::optional<Credential> load(const CredentialRef& ref) override;
    std::vector<CredentialInfo> list(const CredentialRequest& request) const override;
    void remove(const CredentialRef& ref) override;
    void clear() override;

    const std::filesystem::path& path() const { return m_path; }
    std::size_t loadedCount() const { return m_loadedCount; }
    void flush();
    void reload();

    std::filesystem::path credentialPath() const override;
    bool canReloadCredentials() const override;
    void reloadCredentials() override;
    void saveCredentials() override;
    std::size_t credentialPersistedCount() const override;

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

  private:
    void loadFromDisk();
    void saveToDisk();

    std::filesystem::path m_path;
    std::size_t m_loadedCount = 0;
};

} // namespace bas::security

#endif
