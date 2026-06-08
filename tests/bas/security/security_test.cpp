#include "bas/security/AccessDecisionResolver.hpp"
#include "bas/security/Binding.hpp"
#include "bas/security/CredentialManager.hpp"
#include "bas/security/Demo.hpp"
#include "bas/security/IdentityRegistry.hpp"
#include "bas/security/IdentityService.hpp"
#include "bas/security/LoginUi.hpp"
#include "bas/security/PasswordDigest.hpp"
#include "bas/security/Permission.hpp"
#include "bas/security/PolicyStore.hpp"
#include "bas/security/Realm.hpp"
#include "bas/security/SecurityManager.hpp"
#include "bas/security/UserStore.hpp"

#include <bas/reg/JsonRegistry.hpp>

#include <cassert>
#include <iostream>
#include <memory>

using namespace bas::security;

static std::string sessionUser(const SecurityManager& ac, const Realm& realm) {
    for (const auto& id : ac.currentIdentities()) {
        if (id.type == "user" && id.realm.scopedEqual(realm) && isLoginSessionIdentity(id)) {
            return id.name;
        }
    }
    return {};
}

static void test_permission_matcher() {
    DefaultPermissionMatcher matcher;
    assert(matcher.matches(Permission{"file.*"}, Permission{"file.read"}));
    assert(matcher.matches(Permission{"file.*"}, Permission{"file.write"}));
    assert(!matcher.matches(Permission{"file.*"}, Permission{"file.read.local"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file.read"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file.read.local"}));
    assert(matcher.specificity(Permission{"file.read"}) >
           matcher.specificity(Permission{"file.*"}));
    assert(matcher.specificity(Permission{"file.*"}) > matcher.specificity(Permission{"file.**"}));
    assert(!matcher.matches(Permission{"fab.order.modify"}, Permission{"fab.order.delete"}));
}

static void test_resolve_tied_permission_specificity() {
    DefaultACResolvePolicy resolver;
    std::vector<ACEMatch> matches;

    const Realm global = Realm::GLOBAL;
    matches.push_back(ACEMatch{ACEntry{Permission{"file.read"}, AccessEffect::Allow},
                               IdentityRef{"user", global, "a"}, 20, 5});
    matches.push_back(ACEMatch{ACEntry{Permission{"file.*"}, AccessEffect::Deny},
                               IdentityRef{"user", global, "b"}, 20, 3});
    assert(resolver.resolve(matches) == AccessEffect::Deny);

    matches.clear();
    matches.push_back(ACEMatch{ACEntry{Permission{"file.read"}, AccessEffect::Allow},
                               IdentityRef{"user", global, "low"}, 20, 2});
    matches.push_back(ACEMatch{ACEntry{Permission{"file.read"}, AccessEffect::Allow},
                               IdentityRef{"user", global, "high"}, 20, 9});
    assert(resolver.resolve(matches) == AccessEffect::Allow);
}

static void test_credential_from_login_form() {
    LoginFormSpec form;
    form.title = "Sign in";
    LoginField username;
    username.name = "username";
    username.type = "text";
    username.required = true;
    LoginField password;
    password.name = "password";
    password.type = "password";
    password.required = true;
    form.fields = {username, password};

    CredentialRequest request;
    request.types = {"password"};
    request.subjectHint = "alice";
    request.serviceHint = "bas.identity.user.store";

    const auto cred = credentialFromLoginForm(form, request, [](const LoginField& field) {
        if (field.name == "username") {
            return std::optional<std::string>("alice");
        }
        if (field.name == "password") {
            return std::optional<std::string>("secret");
        }
        return std::optional<std::string>{};
    });

    assert(cred.has_value());
    assert(cred->meta.type == "password");
    assert(cred->meta.subjectHint == "alice");
    assert(cred->secret->value() == "secret");
}

static void test_credential_layers() {
    DefaultCredentialManager manager;

    Credential cred;
    cred.ref.store = DefaultCredentialManager::kStore;
    cred.ref.id = "test-1";
    cred.meta.type = "password";
    cred.meta.subjectHint = "alice";
    cred.meta.serviceHint = "bas.identity.user.store";
    cred.secret = SecretValue("secret");

    const auto ref = manager.put(std::move(cred));
    assert(ref.store == DefaultCredentialManager::kStore);
    assert(ref.id == "test-1");

    CredentialRequest req;
    req.types = {"password"};
    req.subjectHint = "alice";
    req.serviceHint = "bas.identity.user.store";

    const auto loaded = manager.load(ref);
    assert(loaded.has_value());
    assert(loaded->meta.type == "password");
    assert(loaded->secret->value() == "secret");

    const auto listed = manager.list(req);
    assert(listed.size() == 1);
    assert(listed[0].ref.id == "test-1");
    assert(listed[0].meta.type == "password");
}

static void test_one_shot_authenticate() {
    auto acl = std::make_shared<DefaultPolicyStore>();
    const Realm deviceRealm{"device", "", "tank-a", "", ""};
    acl->addGrant(makeAccessGrant(IdentityRef{"role", deviceRealm, "operator"},
                                  Permission{"device.forward"}, AccessEffect::Allow));
    acl->addGrant(makeAccessGrant(IdentityRef{"role", deviceRealm, "gunner"},
                                  Permission{"device.fire"}, AccessEffect::Allow));

    auto userStore = std::make_shared<DefaultUserStore>();
    {
        UserRecord alice;
        alice.profile.name = "alice";
        alice.roles = {"operator"};
        alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
        userStore->addUser(alice);
    }
    {
        UserRecord bob;
        bob.profile.name = "bob";
        bob.roles = {"gunner"};
        bob.keys.push_back(makePasswordHashKey("pwd-main", "bob"));
        userStore->addUser(bob);
    }

    auto credentialManager = std::make_shared<DefaultCredentialManager>();
    auto registry = std::make_shared<IdentityRegistry>();
    registry->add(std::make_shared<AnonymousIdentityService>());
    registry->add(std::make_shared<StoreIdentityService>(userStore), deviceRealm);

    auto matcher = std::make_shared<DefaultPermissionMatcher>();
    auto resolver = std::make_shared<DefaultACResolvePolicy>();

    SecurityManager ac(acl, credentialManager, registry, matcher, resolver);

    AccessRequestOptions opts;
    opts.realmHint = deviceRealm;
    opts.allowAutoLogin = false;
    opts.allowConsoleInteraction = false;
    opts.allowGuiInteraction = false;

    Credential aliceCred;
    aliceCred.meta.type = "password";
    aliceCred.meta.subjectHint = "alice";
    aliceCred.meta.realm = deviceRealm;
    aliceCred.secret = SecretValue("alice");
    credentialManager->put(std::move(aliceCred));
    opts.nameHint = "alice";
    assert(ac.login(opts));
    assert(ac.checkPermission(Permission{"device.forward"}, opts) == AccessEffect::Allow);
    assert(ac.checkPermission(Permission{"device.fire"}, opts) == AccessEffect::Deny);

    Credential bobCred;
    bobCred.meta.type = "password";
    bobCred.meta.subjectHint = "bob";
    bobCred.meta.realm = deviceRealm;
    bobCred.secret = SecretValue("bob");

    const auto bobIdentities = ac.authenticate(std::move(bobCred), opts);
    assert(bobIdentities.has_value());
    const Subject bobSubject = Subject::fromIdentitySet(*bobIdentities);
    assert(ac.checkSubjectPermission(Permission{"device.fire"}, bobSubject, opts) ==
           AccessEffect::Allow);
    assert(ac.checkSubjectPermission(Permission{"device.forward"}, bobSubject, opts) ==
           AccessEffect::Deny);

    assert(ac.checkPermission(Permission{"device.fire"}, opts) == AccessEffect::Deny);
    assert(!ac.currentIdentities().empty());
    assert(sessionUser(ac, deviceRealm) == "alice");

    ac.logoutAll();
    assert(ac.checkPermission(Permission{"device.forward"}, opts) == AccessEffect::Deny);

    Credential bobCred2;
    bobCred2.meta.type = "password";
    bobCred2.meta.subjectHint = "bob";
    bobCred2.meta.realm = deviceRealm;
    bobCred2.secret = SecretValue("bob");
    const auto bobIdentities2 = ac.authenticate(std::move(bobCred2), opts);
    assert(bobIdentities2.has_value());
    const Subject bobSubject2 = Subject::fromIdentitySet(*bobIdentities2);
    assert(ac.checkSubjectPermission(Permission{"device.fire"}, bobSubject2, opts) ==
           AccessEffect::Allow);
    ac.activate(*bobIdentities2);
    assert(ac.checkPermission(Permission{"device.fire"}, opts) == AccessEffect::Allow);
    assert(sessionUser(ac, deviceRealm) == "bob");

    bool hasOperator = false;
    bool hasGunner = false;
    for (const auto& id : ac.currentIdentities()) {
        if (id.type != "role" || !id.realm.scopedEqual(deviceRealm)) {
            continue;
        }
        if (id.name == "operator") {
            hasOperator = true;
        }
        if (id.name == "gunner") {
            hasGunner = true;
        }
    }
    assert(!hasOperator);
    assert(hasGunner);
}

static void test_acl_and_manager() {
    auto acl = std::make_shared<DefaultPolicyStore>();
    const Realm global = Realm::GLOBAL;
    acl->addGrant(makeAccessGrant(IdentityRef{"role", global, "operator"},
                                  Permission{"fab.order.modify"}, AccessEffect::Allow));
    acl->addGrant(makeAccessGrant(IdentityRef{"user", global, "bob"},
                                  Permission{"fab.order.delete"}, AccessEffect::Deny));

    auto credentialManager = std::make_shared<DefaultCredentialManager>();
    auto registry = std::make_shared<IdentityRegistry>();
    registry->add(std::make_shared<AnonymousIdentityService>());
    registry->add(std::make_shared<StoreIdentityService>(std::make_shared<DemoUserStore>()));

    auto matcher = std::make_shared<DefaultPermissionMatcher>();
    auto resolver = std::make_shared<DefaultACResolvePolicy>();

    SecurityManager ac(acl, credentialManager, registry, matcher, resolver);
    ac.start();

    assert(ac.checkPermission(Permission{"fab.order.modify"}) == AccessEffect::Deny);

    Identity role;
    role.type = "role";
    role.name = "operator";
    role.state = IdentityState::Active;
    role.source = IdentitySource::Derived;
    role.issuedAt = std::chrono::system_clock::now();

    IdentitySet set;
    set.primary = role;
    set.identities.push_back(role);
    ac.activate(set);

    assert(ac.checkPermission(Permission{"fab.order.modify"}) == AccessEffect::Allow);
    assert(ac.checkPermission(Permission{"fab.order.delete"}) == AccessEffect::Deny);
}

static void test_login_session_realm_switch() {
    auto acl = std::make_shared<DefaultPolicyStore>();
    const Realm deviceA{"device", "", "device-a", "", ""};
    const Realm deviceB{"device", "", "device-b", "", ""};

    auto userStoreA = std::make_shared<DefaultUserStore>();
    {
        UserRecord alice;
        alice.profile.name = "alice";
        alice.roles = {"operator"};
        alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
        userStoreA->addUser(alice);
    }
    {
        UserRecord carol;
        carol.profile.name = "carol";
        carol.roles = {"operator"};
        carol.keys.push_back(makePasswordHashKey("pwd-main", "carol"));
        userStoreA->addUser(carol);
    }

    auto userStoreB = std::make_shared<DefaultUserStore>();
    {
        UserRecord bob;
        bob.profile.name = "bob";
        bob.roles = {"operator"};
        bob.keys.push_back(makePasswordHashKey("pwd-main", "bob"));
        userStoreB->addUser(bob);
    }

    auto credentialManager = std::make_shared<DefaultCredentialManager>();
    auto registry = std::make_shared<IdentityRegistry>();
    registry->add(std::make_shared<StoreIdentityService>(userStoreA), deviceA);
    registry->add(std::make_shared<StoreIdentityService>(userStoreB), deviceB);

    auto matcher = std::make_shared<DefaultPermissionMatcher>();
    auto resolver = std::make_shared<DefaultACResolvePolicy>();
    SecurityManager ac(acl, credentialManager, registry, matcher, resolver);

    AccessRequestOptions optsA;
    optsA.realmHint = deviceA;
    optsA.allowAutoLogin = false;
    optsA.allowConsoleInteraction = false;
    optsA.allowGuiInteraction = false;

    AccessRequestOptions optsB = optsA;
    optsB.realmHint = deviceB;

    Credential aliceCred;
    aliceCred.meta.type = "password";
    aliceCred.meta.subjectHint = "alice";
    aliceCred.meta.realm = deviceA;
    aliceCred.secret = SecretValue("alice");
    credentialManager->put(std::move(aliceCred));
    optsA.nameHint = "alice";
    assert(ac.login(optsA));
    assert(ac.currentIdentities().size() == 2);

    Credential bobCred;
    bobCred.meta.type = "password";
    bobCred.meta.subjectHint = "bob";
    bobCred.meta.realm = deviceB;
    bobCred.secret = SecretValue("bob");
    credentialManager->put(std::move(bobCred));
    optsB.nameHint = "bob";
    assert(ac.login(optsB));
    assert(ac.currentIdentities().size() == 4);

    Credential carolCred;
    carolCred.meta.type = "password";
    carolCred.meta.subjectHint = "carol";
    carolCred.meta.realm = deviceA;
    carolCred.secret = SecretValue("carol");
    credentialManager->put(std::move(carolCred));
    optsA.nameHint = "carol";
    assert(ac.login(optsA));

    std::size_t deviceAUsers = 0;
    std::size_t deviceBUsers = 0;
    for (const auto& id : ac.currentIdentities()) {
        if (id.type != "user") {
            continue;
        }
        if (id.realm.scopedEqual(deviceA)) {
            ++deviceAUsers;
            assert(id.name == "carol");
        }
        if (id.realm.scopedEqual(deviceB)) {
            ++deviceBUsers;
            assert(id.name == "bob");
        }
    }
    assert(deviceAUsers == 1);
    assert(deviceBUsers == 1);
}

static void test_credential_realm_form_defaults() {
    LoginFormSpec form;
    form.title = "User Login";
    LoginField realmType;
    realmType.name = "realm_type";
    realmType.type = "text";
    realmType.options["value"] = "device";
    LoginField realmName;
    realmName.name = "realm";
    realmName.type = "text";
    realmName.options["value"] = "tablet-1";
    LoginField username;
    username.name = "username";
    username.type = "text";
    username.required = true;
    LoginField password;
    password.name = "password";
    password.type = "password";
    password.required = true;
    form.fields = {realmType, realmName, username, password};

    CredentialRequest request;
    request.types = {"password"};
    request.serviceHint = "bas.identity.user.store";

    const auto cred = credentialFromLoginForm(form, request, [](const LoginField& field) {
        if (field.name == "username") {
            return std::optional<std::string>("alice");
        }
        if (field.name == "password") {
            return std::optional<std::string>("secret");
        }
        return std::optional<std::string>{""};
    });

    assert(cred.has_value());
    assert(cred->meta.realm.type == "device");
    assert(cred->meta.realm.name == "tablet-1");
}

static void test_realm_type_matching() {
    Realm stored;
    stored.type = "device";
    stored.name = "tablet-1";

    Realm hintDevice;
    hintDevice.type = "device";
    assert(stored.match(hintDevice));

    Realm hintApp;
    hintApp.type = "app";
    assert(!stored.match(hintApp));

    assert(stored.displayLabel() == "device:tablet-1");
}

static void test_credential_realm_filter() {
    DefaultCredentialManager manager;

    Credential credA;
    credA.meta.type = "password";
    credA.meta.subjectHint = "alice";
    credA.meta.serviceHint = "bas.identity.user.store";
    credA.meta.realm.name = "factory-a";
    credA.secret = SecretValue("secret-a");
    manager.put(std::move(credA));

    Credential credB;
    credB.meta.type = "password";
    credB.meta.subjectHint = "alice";
    credB.meta.serviceHint = "bas.identity.user.store";
    credB.meta.realm.name = "factory-b";
    credB.secret = SecretValue("secret-b");
    manager.put(std::move(credB));

    CredentialRequest req;
    req.types = {"password"};
    req.subjectHint = "alice";
    req.serviceHint = "bas.identity.user.store";
    req.realmHint.name = "factory-a";

    const auto found = manager.get(req);
    assert(found.has_value());
    assert(found->meta.realm.name == "factory-a");
    assert(found->secret->value() == "secret-a");
}

static void test_user_store_profile_and_keys() {
    DemoUserStore store;

    assert(store.hasUser("alice"));
    const auto profile = store.getUserProfile("alice");
    assert(profile.has_value());
    assert(profile->displayName == "Alice");
    assert(profile->attributes.at("department").as_string() == "factory");

    const auto keys = store.getKeysByType("alice", "password-hash");
    assert(keys.size() == 1);
    assert(keys[0].data.at("algorithm").as_string() == "sha256");
    assert(keys[0].data.at("hash").as_string() == digestPassword("sha256", "alice"));

    assert(verifyUserPassword(store, "alice", "alice"));
    assert(!verifyUserPassword(store, "alice", "wrong"));

    const auto record = store.getUserRecord("j");
    assert(record.has_value());
    assert(record->roles.size() == 1);
    assert(record->roles[0] == "operator");
}

static void test_user_record_json_roundtrip() {
    UserRecord record;
    record.profile.name = "tester";
    record.profile.displayName = "Tester";
    record.profile.email = "t@example.com";
    record.roles = {"operator", "auditor"};
    UserAuthKey key;
    key.id = "pwd-1";
    key.type = "password-hash";
    key.name = "Main";
    key.data["algorithm"] = "argon2id";
    key.data["hash"] = "abc";
    key.data["salt"] = "xyz";
    record.keys.push_back(key);

    boost::json::object o;
    record.jsonOut(o, JsonFormOptions::DEFAULT);

    UserRecord parsed;
    parsed.jsonIn(o, JsonFormOptions::DEFAULT);
    assert(parsed.profile.name == "tester");
    assert(parsed.roles.size() == 2);
    assert(parsed.roles[0] == "operator");
    assert(parsed.roles[1] == "auditor");
    assert(parsed.keys[0].data.at("hash").as_string() == "abc");
}

static void test_password_digest_algorithms() {
    assert(digestPassword("md5", "test") == "098f6bcd4621d373cade4e832627b4f6");
    assert(digestPassword("sha1", "test") == "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3");
    assert(digestPassword("sha256", "test") ==
           "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");

    UserAuthKey plain = makePasswordPlainKey("pwd", "secret");
    assert(plain.type == "password-plain");
    assert(passwordMatchesAuthKey("secret", plain, std::chrono::system_clock::now()));
    assert(!passwordMatchesAuthKey("wrong", plain, std::chrono::system_clock::now()));
}

static void test_secret_masked() {
    SecretValue secret("super-secret-value");
    assert(secret.masked().find("super-secret-value") == std::string::npos);
    assert(!secret.masked().empty());
}

static void test_registry_user_store() {
    auto registry = std::make_shared<bas::reg::JsonRegistry>();
    auto store = std::make_shared<RegistryUserStore>(registry, "security/users",
                                                     std::make_shared<DefaultUserStore>());

    UserRecord alice;
    alice.profile.name = "alice";
    alice.profile.displayName = "Alice";
    alice.roles = {"operator"};
    alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
    store->addUser(alice);

    assert(store->hasUser("alice"));
    assert(store->loadedCount() == 1);
    assert(verifyUserPassword(*store, "alice", "alice"));

    auto reloaded = std::make_shared<RegistryUserStore>(registry, "security/users",
                                                        std::make_shared<DefaultUserStore>());
    assert(reloaded->hasUser("alice"));
    assert(reloaded->getRoles("alice").size() == 1);
    assert(verifyUserPassword(*reloaded, "alice", "alice"));

    reloaded->removeUser("alice");
    assert(!reloaded->hasUser("alice"));

    auto empty = std::make_shared<RegistryUserStore>(registry, "security/users",
                                                     std::make_shared<DefaultUserStore>());
    assert(!empty->hasUser("alice"));
}

int main() {
    test_permission_matcher();
    test_resolve_tied_permission_specificity();
    test_credential_from_login_form();
    test_credential_realm_form_defaults();
    test_login_session_realm_switch();
    test_realm_type_matching();
    test_credential_layers();
    test_credential_realm_filter();
    test_user_store_profile_and_keys();
    test_password_digest_algorithms();
    test_user_record_json_roundtrip();
    test_acl_and_manager();
    test_one_shot_authenticate();
    test_secret_masked();
    test_registry_user_store();
    std::cout << "bas.security tests passed\n";
    return 0;
}
