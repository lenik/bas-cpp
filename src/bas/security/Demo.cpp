#include "Demo.hpp"
#include "Binding.hpp"
#include "PasswordDigest.hpp"
#include "PolicyStore.hpp"
#include "UserStore.hpp"

namespace bas::security {
    
std::string DemoUserStore::storeLabel() const { return "demo"; }

void populateDemoUsers(DefaultUserStore& store) {
    {
        UserRecord alice;
        alice.profile.name = "alice";
        alice.profile.displayName = "Alice";
        alice.profile.email = "alice@example.com";
        alice.profile.attributes["department"] = boost::json::value("factory");
        alice.roles = {"operator"};
        alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
        store.addUser(alice);
    }

    {
        UserRecord bob;
        bob.profile.name = "bob";
        bob.profile.displayName = "Bob";
        bob.roles = {"operator"};
        bob.keys.push_back(makePasswordHashKey("pwd-main", "bob"));
        store.addUser(bob);
    }

    {
        UserRecord jUser;
        jUser.profile.name = "j";
        jUser.profile.displayName = "J";
        jUser.roles = {"operator"};
        jUser.keys.push_back(makePasswordHashKey("pwd-main", "k"));
        store.addUser(jUser);
    }

    {
        UserRecord admin;
        admin.profile.name = "admin";
        admin.profile.displayName = "Administrator";
        admin.profile.email = "admin@example.com";
        admin.roles = {"admin", "operator"};
        admin.keys.push_back(makePasswordHashKey("pwd-main", "admin"));
        store.addUser(admin);
    }
}

void populateDemoPolicy(PolicyStore& store) {
    store.clear();
    const Realm& global = Realm::GLOBAL;
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                   Permission{"action=view;resource=fab.order"},
                                   AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                   Permission{"action=modify;resource=fab.order"},
                                   AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"user", global, "bob"},
                                   Permission{"action=delete;resource=fab.order"},
                                   AccessEffect::Deny));
    store.addGrant(makeAccessGrant(IdentityRef{"anonymous", global, "default"},
                                   Permission{"action=view;resource=fab.order"},
                                   AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                   Permission{"action=save;resource=file"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                   Permission{"resource=file"}, AccessEffect::Allow));
}

namespace {

void addRoleAcl(DefaultPolicyStore& store, const std::string& aclId, const std::string& roleName,
                const std::vector<std::pair<std::string, AccessEffect>>& entries) {
    ACList acl;
    acl.id = aclId;
    for (const auto& [perm, effect] : entries) {
        acl.entries.push_back(ACEntry{Permission{perm}, effect});
    }
    store.addAcl(std::move(acl));
    PolicyBinding binding;
    binding.aclId = aclId;
    binding.identity = IdentityRef{"role", Realm{}, roleName};
    store.addBinding(std::move(binding));
}

} // namespace

void populateTankAUsers(DefaultUserStore& store) {
    {
        UserRecord alice;
        alice.profile.name = "alice";
        alice.profile.displayName = "Alice Operator";
        alice.roles = {"operator"};
        alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
        store.addUser(alice);
    }
    {
        UserRecord bob;
        bob.profile.name = "bob";
        bob.profile.displayName = "Bob Gunner";
        bob.roles = {"gunner"};
        bob.keys.push_back(makePasswordHashKey("pwd-main", "bob"));
        store.addUser(bob);
    }
    {
        UserRecord admin;
        admin.profile.name = "admin";
        admin.profile.displayName = "Commander";
        admin.roles = {"commander"};
        admin.keys.push_back(makePasswordHashKey("pwd-main", "admin"));
        store.addUser(admin);
    }
}

void populateTankAPolicy(PolicyStore& store) {
    store.clear();
    auto& policy = static_cast<DefaultPolicyStore&>(store);
    addRoleAcl(policy, "operator-drive", "operator",
               {{"action=start;resource=device", AccessEffect::Allow},
                {"action=forward;resource=device", AccessEffect::Allow},
                {"action=backward;resource=device", AccessEffect::Allow},
                {"action=left;resource=device", AccessEffect::Allow},
                {"action=right;resource=device", AccessEffect::Allow},
                {"action=stop;resource=device", AccessEffect::Allow}});
    store.addGrant(makeAccessGrant(IdentityRef{"role", Realm{}, "gunner"},
                                   Permission{"action=fire;resource=device"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", Realm{}, "gunner"},
                                   Permission{"action=stop;resource=device"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", Realm{}, "commander"},
                                   Permission{"resource=device"}, AccessEffect::Allow));
}

void populateTankBUsers(DefaultUserStore& store) {
    {
        UserRecord charlie;
        charlie.profile.name = "charlie";
        charlie.profile.displayName = "Charlie Cadet";
        charlie.roles = {"cadet"};
        charlie.keys.push_back(makePasswordHashKey("pwd-main", "charlie"));
        store.addUser(charlie);
    }
    {
        UserRecord dana;
        dana.profile.name = "dana";
        dana.profile.displayName = "Dana Instructor";
        dana.roles = {"instructor"};
        dana.keys.push_back(makePasswordHashKey("pwd-main", "dana"));
        store.addUser(dana);
    }
}

void populateTankBPolicy(PolicyStore& store) {
    store.clear();
    auto& policy = static_cast<DefaultPolicyStore&>(store);
    addRoleAcl(policy, "cadet-training", "cadet",
               {{"action=start;resource=device", AccessEffect::Allow},
                {"action=forward;resource=device", AccessEffect::Allow},
                {"action=backward;resource=device", AccessEffect::Allow},
                {"action=left;resource=device", AccessEffect::Allow},
                {"action=right;resource=device", AccessEffect::Allow},
                {"action=stop;resource=device", AccessEffect::Allow},
                {"action=fire;resource=device", AccessEffect::Deny}});
    store.addGrant(makeAccessGrant(IdentityRef{"role", Realm{}, "instructor"},
                                   Permission{"resource=device"}, AccessEffect::Allow));
}

} // namespace bas::security