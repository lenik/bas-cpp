#include "PublicAccess.hpp"

#include "Binding.hpp"
#include "Realm.hpp"

namespace bas::security {
namespace {

std::shared_ptr<DefaultUserStore> makeUsers() {
    auto store = std::make_shared<DefaultUserStore>();
    UserRecord anonymous;
    anonymous.profile.name = "anonymous";
    anonymous.profile.displayName = "Anonymous";
    anonymous.profile.enabled = true;
    store->addUser(anonymous);

    UserRecord publicUser;
    publicUser.profile.name = "public";
    publicUser.profile.displayName = "Public";
    publicUser.profile.enabled = true;
    store->addUser(publicUser);
    return store;
}

std::shared_ptr<DefaultPolicyStore> makePolicy() {
    auto store = std::make_shared<DefaultPolicyStore>();
    // Empty permission = match everything. Empty realm on IdentityRef is a wildcard.
    store->addGrant(makeAccessGrant(IdentityRef{"anonymous", Realm{}, "default"}, Permission{},
                                    AccessEffect::Allow));
    store->addGrant(makeAccessGrant(IdentityRef{"public", Realm{}, "default"}, Permission{},
                                    AccessEffect::Allow));
    return store;
}

struct Holds {
    std::shared_ptr<DefaultUserStore> users = makeUsers();
    std::shared_ptr<DefaultPolicyStore> policy = makePolicy();
};

Holds& holds() {
    static Holds h;
    return h;
}

} // namespace

std::shared_ptr<UserStore> PublicAccess::userStore() { return holds().users; }

std::shared_ptr<PolicyStore> PublicAccess::policyStore() { return holds().policy; }

bool PublicAccess::isPublicUserStore(const std::shared_ptr<UserStore>& store) {
    return store && store.get() == holds().users.get();
}

bool PublicAccess::isPublicPolicyStore(const std::shared_ptr<PolicyStore>& store) {
    return store && store.get() == holds().policy.get();
}

} // namespace bas::security
