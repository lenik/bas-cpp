#ifndef APP_TANKS_RBAC_HPP
#define APP_TANKS_RBAC_HPP

#include <bas/security/CredentialManager.hpp>
#include <bas/security/IdentityRegistry.hpp>
#include <bas/security/Permission.hpp>
#include <bas/security/PolicyStore.hpp>
#include <bas/security/Realm.hpp>
#include <bas/security/SecurityManager.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sec = bas::security;

struct DeviceSlot {
    sec::Realm realm;
    std::string name;
    std::string label;
};

struct DemoContext {
    std::shared_ptr<sec::DefaultCredentialManager> credentials;
    std::shared_ptr<sec::RealmPolicyStore> policyStore;
    std::shared_ptr<sec::IdentityRegistry> registry;
    std::shared_ptr<sec::SecurityManager> sm;
    std::vector<DeviceSlot> devices;
};

struct OpResult {
    bool allowed = false;
    std::string op;
    std::string message;
};

inline const std::unordered_map<std::string, std::string> kOpPermissions = {
    {"start", "device.start"}, {"forward", "device.forward"}, {"backward", "device.backward"},
    {"left", "device.left"},   {"right", "device.right"},     {"fire", "device.fire"},
    {"stop", "device.stop"},
};

struct DemoAccount {
    const char* user;
    const char* password;
    const char* role;
};

std::string trim(std::string_view text);
std::vector<std::string> tokenize(std::string_view line);

std::string primaryUser(const sec::SecurityManager& sm, const sec::Realm& realm);
std::string primaryRole(const sec::SecurityManager& sm, const sec::Realm& realm);
std::string formatIdentity(const sec::IdentityRef& id);

const sec::DefaultPolicyStore* defaultPolicyFor(const DemoContext& ctx, const sec::Realm& realm);
bool identityMatchesCurrent(const DemoContext& ctx, const sec::Realm& realm,
                            const sec::IdentityRef& ref);
bool isActiveBinding(const DemoContext& ctx, const sec::Realm& realm,
                     const sec::PolicyBinding& binding);
int effectColorPair(const sec::AccessEffect& effect);
bool isWildcardPermission(const sec::Permission& permission);
bool isAclActiveForCurrent(const DemoContext& ctx, const sec::Realm& realm,
                           const sec::DefaultPolicyStore* policy, const std::string& aclId);
bool shouldHighlightGrant(const DemoContext& ctx, const sec::Realm& realm,
                          const sec::AccessGrant& grant);

DemoContext makeContext();
OpResult requestOp(DemoContext& ctx, const DeviceSlot& device, const std::string& op);
bool loginUser(DemoContext& ctx, const DeviceSlot& device, const std::string& user,
               const std::string& password);

const std::vector<DemoAccount>& demoAccountsFor(const DeviceSlot& device);

std::optional<std::size_t> findDevice(const DemoContext& ctx, const std::string& name);
bool batchLogin(DemoContext& ctx, const DeviceSlot& device, const std::string& user,
                const std::string& password);
bool batchHandle(DemoContext& ctx, const std::vector<std::string>& args, std::size_t& active);
void runBatch(DemoContext& ctx);

#endif // APP_TANKS_RBAC_HPP
