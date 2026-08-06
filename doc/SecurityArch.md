# Security / Access-Control Architecture

Namespace: `bas::security`  
Code: `src/bas/security/`  
Demos: `app/acdemo.cpp`, `app/tanks_rbac.*`, `app/tanks_game.*`  
Tests: `tests/bas/security/security_test.cpp`

中文版：[SecurityArch-zh_CN.md](SecurityArch-zh_CN.md)

This document describes the access-control model implemented by bas-cpp: identities, realms, policy stores, session lifecycle, and how applications wire them together.

---

## 1. Goals

- Separate **authentication** (who you are) from **authorization** (what you may do).
- Support **multiple realms** (devices, tenants, apps) in one process with independent login sessions.
- Express policy as **named ACLs**, **bindings**, and **direct grants**.
- Allow **check-only** permission tests (no prompts) and **request** flows that may login interactively.
- Support **one-shot elevation**: verify another subject’s permission without replacing the active session.

There is **no** concurrency/login-policy table. Session replacement is driven by **identity source** (`IdentitySource::Login`) and realm scope.

---

## 2. Concepts

### Realm

A realm is a tenancy / device / app scope (`Realm`: `type`, `uuid`, `name`, …).

| Method | Meaning |
|--------|---------|
| `match(hint)` | Filter: empty fields in the hint are wildcards. |
| `same(other)` | Identity/ACL equality: empty on either side is a wildcard. |
| `scopedEqual(other)` | Strict slot equality for login sessions (empty ≡ empty only). |
| `storageKey()` | Stable map key (`uuid:…` / `type:name` / …). |

`Realm::GLOBAL` is the default shared realm for single-store demos.

### Identity

An identity is a typed principal in a realm:

```text
IdentityRef = { type, realm, name }   // e.g. user:alice @ device:tank-a
Identity    = IdentityRef + displayName, serviceId, source, state, times, attributes
IdentitySet = optional primary + vector of identities
```

Common types: `user`, `role`, `anonymous`, `public`, …

**Source** (`IdentitySource`) marks provenance and drives session cleanup:

| Source | Typical use | Cleared by `logoutRealm` / login switch? |
|--------|-------------|------------------------------------------|
| `Login` | Password login (`StoreIdentityService`) | Yes |
| `Auto` | Anonymous / public auto-login | No |
| `Derived` / `Direct` / `System` | Manual activate, system principals | No (unless matching `IdentityRef`) |

Helper: `isLoginSessionIdentity(id)` ⟺ `source == Login`.

### Subject

A `Subject` is the authorization-time view of a set of identities (flattened from an `IdentitySet`). Checks either use the **active session** subject or an **explicit** subject (one-shot elevation).

### Permission

`Permission` has optional `action` and `resource`. **Empty field means all.**

Canonical text form (`;`-separated key/value pairs; values may be quoted):

```text
action=<action>;resource=<resource>
```

Examples:

- `action=fire;resource=device` — fire on device
- `resource=device` — any action on device
- `action=view;resource=fab.order` — view fab.order
- `action=read;resource="file;special"` — quoted value containing `;`
- _(empty)_ — match everything

Action wildcards (dot-separated tokens): `*` one token, `**` zero-or-more.  
Legacy shorthand without `=` (`file.read` or `file.read:res`) is still accepted by `parse()`.

Specificity prefers literals over `*` over `**`; used when resolving conflicting ACEs.

### Access effect

`AccessEffect`: `Unknown` | `Allow` | `Deny`.

- Policy lookup may return **Unknown** (no decisive ACE).
- Public `checkPermission*` APIs **normalize Unknown → Deny**.
- `requestPermission` treats Unknown as “try harder” (auto-login / interactive login).

---

## 3. Component overview

```text
                         LoginUi (optional)
                              |
  CredentialManager <--- SecurityManager ---> PolicyStore (+ RealmPolicyStore)
                              |
                       IdentityRegistry
                     +--------+--------+
                     |        |        |
               AnonymousIS  StoreIS  StoreIS
                            @realmA  @realmB
                                |
                            UserStore
```

| Component | Responsibility |
|-----------|----------------|
| **SecurityManager** | Active session, login/logout, permission checks, optional interactive elevation. |
| **PolicyStore** | ACLists, bindings, grants; `effectiveEntriesOf(IdentityRef)`. |
| **RealmPolicyStore** | Routes policy ops by identity/realm to per-realm stores (+ global). |
| **IdentityRegistry** | Maps realm slots → `IdentityService`s; discovers auto-login services. |
| **IdentityService** | Authenticate → `IdentitySet`. |
| **UserStore** | Durable user records (not session state). |
| **CredentialManager** | Client-side credential cache (secrets for login). |
| **LoginUi** | Collect credentials when login needs interaction. |
| **PermissionMatcher** / **ACResolvePolicy** | Match permissions; resolve Allow/Deny among ACE matches. |

---

## 4. Policy model

Policy is three layers (all optional):

1. **ACList** — named list of `{permission, effect}` with **no** identity.
2. **PolicyBinding** — binds an ACList id to an `IdentityRef` (usually a role).
3. **AccessGrant** — direct `{identity, permission, effect}`.

Effective ACEs for an identity:

```text
effectiveEntriesOf(id) = grantsOf(id) ∪ entries of all ACLs bound to id
```

### Resolution (`DefaultACResolvePolicy`)

Among matching ACEs:

1. Prefer highest **permission specificity**.
2. At that specificity, **Deny wins** over Allow.
3. Among Allows, prefer higher **identity specificity** (`10 + IdentitySource::priority()`).

`DefaultAccessDecisionResolver` (used by `evaluate()`) applies a similar deny-wins rule over raw `ACEntry` lists.

### RealmPolicyStore

- `addRealmStore(realm, store)` / `setGlobalStore(store)`.
- Lookups for grants/bindings/`effectiveEntriesOf` resolve via `identity.realm` → keyed store, else global.
- ACL id queries scan realm stores then global; `addAcl` writes the global store unless routed by a higher layer.

This is how dual-device demos keep **tank-a** and **tank-b** ACLs separate under one `SecurityManager`.

---

## 5. Session lifecycle

Session state lives only in `SecurityManager`:

- `m_activeIdentities` — currently active principals
- `m_suppressAutoLogin` — set on logout; cleared on successful interactive login

### activate(IdentitySet)

1. Collect realms of incoming **Login** identities.
2. For each such realm, `clearLoginSession(realm)` (drop prior Login identities in that realm).
3. Upsert the new identities (primary first; dedupe by `IdentityRef`).

Implication: switching user on the same device **replaces** that realm’s login session (user + roles). Other realms and Auto identities remain.

### logout*

| API | Effect |
|-----|--------|
| `logout(ref)` | Remove one identity; suppress auto-login. |
| `logoutType(type)` | Remove all of that type. |
| `logoutRealm(realm)` | Clear **Login** identities with `scopedEqual(realm)`. |
| `logoutAll()` | Clear everything; suppress auto-login. |

### login vs authenticate

| API | Session change? | Use |
|-----|-----------------|-----|
| `login(options)` | Yes (`activate`) | Interactive / cached credential login. |
| `authenticate(cred, options)` | **No** | Verify credentials → `IdentitySet` for one-shot checks. |
| `activateCachedCredentials` | Yes | Replay stored credentials at startup / login. |

### Permission APIs

| API | Behavior |
|-----|----------|
| `checkPermission` | Against active session (optional `realmHint` filter). Never prompts. Unknown→Deny. |
| `checkSubjectPermission` | Against an explicit `Subject`. Never prompts. No session mutation. |
| `requestPermission` | If Unknown: may auto-login and/or interactive LoginUi; then recheck. |
| `requirePermission` | `requestPermission`; throws `AccessDenied` if not Allow. |
| `evaluate` | Structured `AccessDecision` via `AccessDecisionResolver` (no login side effects). |

### start()

Non-interactive auto-login only (`AnonymousIdentityService`, etc.). Does not prompt.

---

## 6. Identity services

### AnonymousIdentityService

- `canAutoLogin() == true`
- Emits `anonymous:default` and `public:default` with `IdentitySource::Auto`
- Used for guest-visible grants (e.g. demo `action=view;resource=fab.order`)

### StoreIdentityService

- Backed by a `UserStore`
- Password credential → verify hash → emit:
  - `user:<name>` (`IdentitySource::Login`)
  - one `role:<role>` per assigned role (`IdentitySource::Login`)
- Realm taken from credential / request hint

### Extension point

Implement `IdentityService` (`login` / `logout` / optional `refresh`), register via `IdentityRegistry::add(service, realm?)`. Unbound services (no realm slot) are global helpers (anonymous). Bound services own a realm slot (`scopedEqual`).

---

## 7. Credentials vs user auth keys

| Store | Contents | Role |
|-------|----------|------|
| **CredentialManager** | Client-held secrets (password, etc.) + hints | What login uses to authenticate |
| **UserStore keys** | Server-side verification material (`password-hash`, …) | What `StoreIdentityService` checks |

They are intentionally separate. File-backed demo stores keep hashes/secrets in JSON and are **not** production secret stores.

---

## 8. Application wiring

### acdemo — single-store REPL

- One `PolicyStore` (demo or file) + one `UserStore`
- Registry: Anonymous (unbound) + `StoreIdentityService` on `GLOBAL` (and optional CLI default realm)
- `ConsoleLogin` for interactive prompts
- Commands: `login`, `whoami`, `check`, `request`, policy/user CRUD

### tanks_game / tanks_rbac — dual-device demo

```text
RealmPolicyStore
  ├─ device:tank-a → TankAPolicyStore   + StoreIS(TankAUserStore)
  └─ device:tank-b → TankBPolicyStore   + StoreIS(TankBUserStore)
```

- Built-in C++ demo data (`TankA*Store` / `TankB*Store` in `Demo.*`) — no external fixture files required.
- Ops map to permissions (`action=forward;resource=device`, `action=fire;resource=device`, …).
- Game path uses **`checkPermission` only** (no auto prompt on move/fire).
- Denied op → elevation dialog:
  1. `authenticate(cred)`
  2. `checkSubjectPermission(perm, subject)`
  3. If session empty → `activate` (login); else allow **one-shot** without replacing session.
- Explicit login (`L` / batch `login`) calls `login()`; prior Login session for that realm is cleared inside `activate`.

---

## 9. Design invariants

1. **Login session ≡ `IdentitySource::Login`.** Auto/anonymous identities are not cleared by realm login switch.
2. **At most one Login session per realm** in a given `SecurityManager` (activate clears that realm first).
3. **Independent Login sessions across realms** coexist (e.g. logged into tank-a and tank-b).
4. **One-shot elevation** uses `authenticate` + `checkSubjectPermission` without `activate`.
5. **Logout suppresses auto-login** until a successful interactive/`login` path clears the flag.
6. **`check*` never prompts**; **`request*` may**.
7. **Deny wins** at equal max permission specificity.
8. **UserStore roles are names only**; realm is applied when roles become identities at login.
9. **RealmPolicyStore** isolates policy data per device/tenant under one manager.

---

## 10. File map

```text
src/bas/security/
  Types.*                 IdentitySource/State, LoginStatus, AccessEffect
  Realm.*                 Realm + GLOBAL
  Identity.*              IdentityRef / Identity / IdentitySet
  Subject.hpp             Subject helpers
  Permission.*            Permission + DefaultPermissionMatcher
  ACList.*                ACEntry, ACList
  Binding.*               AccessGrant, PolicyBinding, ACEMatch
  AccessDecision.hpp      AccessDecision, AccessRequestOptions
  AccessDecisionResolver.*  Decision + ACE resolve policies
  AccessDenied.hpp
  PolicyStore.*           Default / File / Decorated / RealmPolicyStore
  UserStore.*             Default / File / RegistryUserStore
  PasswordDigest.*
  CredentialManager.*
  IdentityService.*       Anonymous + Store
  IdentityRegistry.*
  LoginUi.*
  SecurityManager.*
  Demo.*                  Demo + TankA/B seed stores
  CommandSupport.*        CLI helpers
  *Command.cpp            CLI implementations
```

---

## 11. Minimal wiring sketch

```cpp
auto policy = std::make_shared<RealmPolicyStore>();
policy->addRealmStore(deviceRealm, std::make_shared<TankAPolicyStore>());

auto creds = std::make_shared<DefaultCredentialManager>();
auto registry = std::make_shared<IdentityRegistry>();
registry->add(std::make_shared<AnonymousIdentityService>());
registry->add(std::make_shared<StoreIdentityService>(userStore), deviceRealm);

auto sm = std::make_shared<SecurityManager>(
    policy, creds, registry,
    std::make_shared<DefaultPermissionMatcher>(),
    std::make_shared<DefaultACResolvePolicy>());
sm->setLoginUi(std::make_shared<ConsoleLogin>(*sm));
sm->start();  // anonymous auto-login if applicable
```

---

## 12. Related entry points

| Artifact | Purpose |
|----------|---------|
| `app/acdemo` | Interactive security REPL |
| `app/tanks_game` | Ncurses dual-device AC demo |
| `tests/bas/security/security_test` | Matcher, session switch, one-shot auth, stores |
