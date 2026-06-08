#include "tanks_rbac.hpp"

#include <bas/security/AccessDecisionResolver.hpp>
#include <bas/security/Demo.hpp>
#include <bas/security/IdentityService.hpp>
#include <bas/security/LoginUi.hpp>
#include <bas/security/UserStore.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

std::string trim(std::string_view text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> tokens;
    std::istringstream iss{std::string(line)};
    std::string word;
    while (iss >> word) {
        tokens.push_back(word);
    }
    return tokens;
}

std::string primaryUser(const sec::SecurityManager& sm, const sec::Realm& realm) {
    for (const auto& id : sm.currentIdentities()) {
        if (id.type == "user" && id.realm.match(realm)) {
            return id.name;
        }
    }
    return "(none)";
}

std::string primaryRole(const sec::SecurityManager& sm, const sec::Realm& realm) {
    for (const auto& id : sm.currentIdentities()) {
        if (id.type == "role" && id.realm.match(realm)) {
            return id.name;
        }
    }
    return "-";
}

std::string formatIdentity(const sec::IdentityRef& id) { return id.type + ":" + id.name; }

const sec::DefaultPolicyStore* defaultPolicyFor(const DemoContext& ctx, const sec::Realm& realm) {
    const auto store = ctx.policyStore->storeForRealm(realm);
    if (!store) {
        return nullptr;
    }
    const sec::PolicyStore* raw = store.get();
    if (const auto* file = dynamic_cast<const sec::FilePolicyStore*>(raw)) {
        raw = file->wrapped();
    }
    return dynamic_cast<const sec::DefaultPolicyStore*>(raw);
}

bool identityMatchesCurrent(const DemoContext& ctx, const sec::Realm& realm,
                            const sec::IdentityRef& ref) {
    for (const auto& id : ctx.sm->currentIdentities()) {
        if (id.ref() == ref && id.realm.match(realm)) {
            return true;
        }
    }
    return false;
}

bool isActiveBinding(const DemoContext& ctx, const sec::Realm& realm,
                     const sec::PolicyBinding& binding) {
    return identityMatchesCurrent(ctx, realm, binding.identity);
}

int effectColorPair(const sec::AccessEffect& effect) {
    if (effect.isAllow()) {
        return 1;
    }
    if (effect.isDeny()) {
        return 3;
    }
    return 0;
}

bool isWildcardPermission(const sec::Permission& permission) {
    return permission.toString().find('*') != std::string::npos;
}

bool isAclActiveForCurrent(const DemoContext& ctx, const sec::Realm& realm,
                           const sec::DefaultPolicyStore* policy, const std::string& aclId) {
    if (!policy) {
        return false;
    }
    for (const auto& binding : policy->bindings()) {
        if (binding.aclId == aclId && isActiveBinding(ctx, realm, binding)) {
            return true;
        }
    }
    return false;
}

bool shouldHighlightGrant(const DemoContext& ctx, const sec::Realm& realm,
                          const sec::AccessGrant& grant) {
    if (!identityMatchesCurrent(ctx, realm, grant.identity)) {
        return false;
    }
    return !isWildcardPermission(grant.permission);
}

DemoContext makeContext() {
    DemoContext ctx;
    ctx.credentials = std::make_shared<sec::DefaultCredentialManager>();
    ctx.policyStore = std::make_shared<sec::RealmPolicyStore>();
    ctx.registry = std::make_shared<sec::IdentityRegistry>();
    ctx.registry->add(std::make_shared<sec::AnonymousIdentityService>());

    auto addDevice = [&](const std::string& name, const std::string& label,
                         std::shared_ptr<sec::DefaultUserStore> userStore,
                         std::shared_ptr<sec::DefaultPolicyStore> policy) {
        DeviceSlot slot;
        slot.name = name;
        slot.label = label;
        slot.realm.type = "device";
        slot.realm.name = name;

        ctx.registry->add(std::make_shared<sec::StoreIdentityService>(userStore), slot.realm);
        ctx.policyStore->addRealmStore(slot.realm, policy);
        ctx.devices.push_back(std::move(slot));
    };

    addDevice("tank-a", "Tank A", std::make_shared<sec::TankAUserStore>(),
              std::make_shared<sec::TankAPolicyStore>());
    addDevice("tank-b", "Tank B", std::make_shared<sec::TankBUserStore>(),
              std::make_shared<sec::TankBPolicyStore>());

    auto matcher = std::make_shared<sec::DefaultPermissionMatcher>();
    auto resolver = std::make_shared<sec::DefaultACResolvePolicy>();

    ctx.sm = std::make_shared<sec::SecurityManager>(ctx.policyStore, ctx.credentials, ctx.registry,
                                                    matcher, resolver);
    ctx.sm->setLoginUi(std::make_shared<sec::ConsoleLogin>(*ctx.sm));
    ctx.sm->start();
    return ctx;
}

OpResult requestOp(DemoContext& ctx, const DeviceSlot& device, const std::string& op) {
    OpResult result;
    result.op = op;
    const auto permIt = kOpPermissions.find(op);
    if (permIt == kOpPermissions.end()) {
        result.message = "unknown op: " + op;
        return result;
    }

    sec::AccessRequestOptions opts;
    opts.realmHint = device.realm;
    opts.allowConsoleInteraction = false;
    opts.allowGuiInteraction = false;
    opts.allowAutoLogin = false;
    opts.reason = "device " + op;

    const sec::Permission permission{permIt->second};
    if (ctx.sm->checkPermission(permission, opts) == sec::AccessEffect::Allow) {
        result.allowed = true;
        result.message = device.name + ": " + op + " OK";
        return result;
    }
    result.message = "DENIED " + device.name + " " + op + " (" + permission.toString() + ")";
    return result;
}

bool loginUser(DemoContext& ctx, const DeviceSlot& device, const std::string& user,
               const std::string& password) {
    sec::Credential cred;
    cred.meta.type = "password";
    cred.meta.subjectHint = user;
    cred.meta.serviceHint = "bas.identity.user.store";
    cred.meta.realm = device.realm;
    cred.secret = sec::SecretValue(password);
    ctx.credentials->put(std::move(cred));

    sec::AccessRequestOptions opts;
    opts.realmHint = device.realm;
    opts.nameHint = user;
    opts.allowConsoleInteraction = false;
    opts.allowAutoLogin = false;
    return ctx.sm->login(opts);
}

const std::vector<DemoAccount>& demoAccountsFor(const DeviceSlot& device) {
    static const std::vector<DemoAccount> kTankA = {
        {"alice", "alice", "operator"},
        {"bob", "bob", "gunner"},
        {"admin", "admin", "commander"},
    };
    static const std::vector<DemoAccount> kTankB = {
        {"charlie", "charlie", "cadet"},
        {"dana", "dana", "instructor"},
    };
    if (device.name == "tank-b") {
        return kTankB;
    }
    return kTankA;
}

std::optional<std::size_t> findDevice(const DemoContext& ctx, const std::string& name) {
    for (std::size_t i = 0; i < ctx.devices.size(); ++i) {
        if (ctx.devices[i].name == name || ctx.devices[i].realm.name == name) {
            return i;
        }
    }
    return std::nullopt;
}

bool batchLogin(DemoContext& ctx, const DeviceSlot& device, const std::string& user,
                const std::string& password) {
    if (loginUser(ctx, device, user, password)) {
        std::cout << "已登录 " << device.name << " as " << user << '\n';
        return true;
    }
    std::cout << "登录失败\n";
    return false;
}

bool batchHandle(DemoContext& ctx, const std::vector<std::string>& args, std::size_t& active) {
    if (args.empty()) {
        return true;
    }
    const std::string& cmd = args[0];
    if (cmd == "quit" || cmd == "exit" || cmd == "q") {
        return false;
    }
    if (cmd == "login" && args.size() >= 4) {
        if (const auto idx = findDevice(ctx, args[1])) {
            return batchLogin(ctx, ctx.devices[*idx], args[2], args[3]) || true;
        }
    }
    if (const auto devIdx = findDevice(ctx, cmd)) {
        if (args.size() >= 2) {
            OpResult res = requestOp(ctx, ctx.devices[*devIdx], args[1]);
            std::cout << (res.allowed ? "✓ " : "✗ ") << res.message << '\n';
            return true;
        }
    }
    if (kOpPermissions.count(cmd) != 0) {
        OpResult res = requestOp(ctx, ctx.devices[active], cmd);
        std::cout << (res.allowed ? "✓ " : "✗ ") << res.message << '\n';
        return true;
    }
    if (cmd == "whoami") {
        for (const auto& d : ctx.devices) {
            std::cout << d.name << " user=" << primaryUser(*ctx.sm, d.realm)
                      << " role=" << primaryRole(*ctx.sm, d.realm) << '\n';
        }
        return true;
    }
    return true;
}

void runBatch(DemoContext& ctx) {
    std::size_t active = 0;
    std::string line;
    while (std::getline(std::cin, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (!batchHandle(ctx, tokenize(line), active)) {
            break;
        }
    }
}
