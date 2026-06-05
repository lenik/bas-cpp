#include "bas/security/SecurityManager.hpp"
#include "bas/security/LoginUi.hpp"
#include "bas/security/AccessDecisionResolver.hpp"
#include "bas/security/Binding.hpp"
#include "bas/security/PolicyStore.hpp"
#include "bas/security/CredentialManager.hpp"
#include "bas/security/IdentityRegistry.hpp"
#include "bas/security/IdentityService.hpp"
#include "bas/security/LoginPolicy.hpp"
#include "bas/security/Permission.hpp"
#include "bas/security/Realm.hpp"
#include "bas/security/PasswordDigest.hpp"
#include "bas/security/UserStore.hpp"

#include <bas/reg/JsonRegistry.hpp>

#include <cassert>
#include <iostream>
#include <memory>

using namespace bas::security;

static void test_permission_matcher() {
    DefaultPermissionMatcher matcher;
    assert(matcher.matches(Permission{"file.*"}, Permission{"file.read"}));
    assert(matcher.matches(Permission{"file.*"}, Permission{"file.write"}));
    assert(!matcher.matches(Permission{"file.*"}, Permission{"file.read.local"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file.read"}));
    assert(matcher.matches(Permission{"file.**"}, Permission{"file.read.local"}));
    assert(matcher.specificity(Permission{"file.read"}) > matcher.specificity(Permission{"file.*"}));
    assert(matcher.specificity(Permission{"file.*"}) > matcher.specificity(Permission{"file.**"}));
    assert(!matcher.matches(Permission{"fab.order.modify"}, Permission{"fab.order.delete"}));
}

static void test_resolve_tied_permission_specificity() {
    DefaultACResolvePolicy resolver;
    std::vector<ACEMatch> matches;

    const Realm global = Realm::GLOBAL;
    matches.push_back(ACEMatch{
        ACEntry{Permission{"file.read"}, AccessEffect::Allow}, IdentityRef{"user", global, "a"}, 20,
        5});
    matches.push_back(ACEMatch{
        ACEntry{Permission{"file.*"}, AccessEffect::Deny}, IdentityRef{"user", global, "b"}, 20,
        3});
    assert(resolver.resolve(matches) == AccessEffect::Deny);

    matches.clear();
    matches.push_back(ACEMatch{
        ACEntry{Permission{"file.read"}, AccessEffect::Allow}, IdentityRef{"user", global, "low"},
        20, 2});
    matches.push_back(ACEMatch{
        ACEntry{Permission{"file.read"}, AccessEffect::Allow}, IdentityRef{"user", global, "high"},
        20, 9});
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

    auto loginPolicy = std::make_shared<LoginPolicy>();
    auto matcher = std::make_shared<DefaultPermissionMatcher>();
    auto resolver = std::make_shared<DefaultACResolvePolicy>();

    SecurityManager ac(acl, credentialManager, registry, loginPolicy, matcher, resolver);
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

static void test_login_policy_multi_realm() {
    LoginPolicy policy;
    Identity aliceA;
    aliceA.type = "user";
    aliceA.name = "alice";
    aliceA.realm.name = "device-a";

    Identity bobB;
    bobB.type = "user";
    bobB.name = "bob";
    bobB.realm.name = "device-b";

    std::vector<Identity> active{aliceA};
    assert(policy.canCoexist(active, bobB));
    assert(policy.resolveConflict(active, bobB) == LoginConflictAction::KeepExisting);

    Identity aliceB;
    aliceB.type = "user";
    aliceB.name = "alice";
    aliceB.realm.name = "device-b";
    assert(policy.canCoexist(active, aliceB));

    Identity carolA;
    carolA.type = "user";
    carolA.name = "carol";
    carolA.realm.name = "device-a";
    assert(!policy.canCoexist(active, carolA));
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
    auto store = std::make_shared<RegistryUserStore>(
        registry, "security/users", std::make_shared<DefaultUserStore>());

    UserRecord alice;
    alice.profile.name = "alice";
    alice.profile.displayName = "Alice";
    alice.roles = {"operator"};
    alice.keys.push_back(makePasswordHashKey("pwd-main", "alice"));
    store->addUser(alice);

    assert(store->hasUser("alice"));
    assert(store->loadedCount() == 1);
    assert(verifyUserPassword(*store, "alice", "alice"));

    auto reloaded = std::make_shared<RegistryUserStore>(
        registry, "security/users", std::make_shared<DefaultUserStore>());
    assert(reloaded->hasUser("alice"));
    assert(reloaded->getRoles("alice").size() == 1);
    assert(verifyUserPassword(*reloaded, "alice", "alice"));

    reloaded->removeUser("alice");
    assert(!reloaded->hasUser("alice"));

    auto empty = std::make_shared<RegistryUserStore>(
        registry, "security/users", std::make_shared<DefaultUserStore>());
    assert(!empty->hasUser("alice"));
}

int main() {
    test_permission_matcher();
    test_resolve_tied_permission_specificity();
    test_credential_from_login_form();
    test_credential_realm_form_defaults();
    test_login_policy_multi_realm();
    test_realm_type_matching();
    test_credential_layers();
    test_credential_realm_filter();
    test_user_store_profile_and_keys();
    test_password_digest_algorithms();
    test_user_record_json_roundtrip();
    test_acl_and_manager();
    test_secret_masked();
    test_registry_user_store();
    std::cout << "bas.security tests passed\n";
    return 0;
}
