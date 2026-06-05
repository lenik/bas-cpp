#include "Demo.hpp"
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
                                   Permission{"fab.order.view"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                   Permission{"fab.order.modify"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"user", global, "bob"},
                                   Permission{"fab.order.delete"}, AccessEffect::Deny));
    store.addGrant(makeAccessGrant(IdentityRef{"anonymous", global, "default"},
                                   Permission{"fab.order.view"}, AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"}, Permission{"file.save"},
                                   AccessEffect::Allow));
    store.addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"}, Permission{"file.*"},
                                   AccessEffect::Allow));
}

} // namespace bas::security