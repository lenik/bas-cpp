# bas.security Package Specification

## 1. Package Goal

`bas.security` 提供一套轻量但可扩展的认证与授权框架。

核心目标：

```text
1. 使用 qualified-token 表达权限。
2. 使用 ACList / ACE 表达授权规则。
3. 使用 identity-type + name 表达权限主体。
4. 使用 CredentialManager 缓存和管理凭证。
5. 使用 IdentityService 完成登录，并返回 IdentitySet。
6. 使用 ACManager 作为应用请求权限的唯一入口。
7. 支持按需登录、自动登录、多个 identity 同时激活（可按 realm 区分设备/租户）。
8. requestPermission / checkPermission 默认 deny。
```

整体调用链：

```text
Application Operation
    -> ACManager.requestPermission(qToken)
        -> check active identities
        -> try auto login
        -> ask CredentialManager
        -> call IdentityService.login(...)
        -> activate IdentitySet
        -> check ACL again
        -> return allow / deny
```

---

# 2. Namespace

```cpp
namespace bas::security {
}
```

---

# 3. Permission Model

## 3.1 Permission Syntax

权限使用 `qualified-token`：

```text
permission:
    qualified-token

qualified-token:
    token ("." token)*

token:
    identifier
    "*"
    "**"
```

示例：

```text
file.read
file.write
file.*
file.**
network.send
fab.order.modify
fab.order.*
```

## 3.2 Token Meaning

```text
*   matches exactly one token level
**  matches zero or more following token levels
```

Examples:

```text
file.*      matches file.read
file.*      matches file.write
file.*      does not match file.read.local

file.**     matches file
file.**     matches file.read
file.**     matches file.read.local

fab.order.* matches fab.order.modify
fab.order.* does not match fab.order.modify.price
```

## 3.3 C++ Type

```cpp
using Permission = std::string;
using QualifiedToken = std::string;
```

---

# 4. Access Control Mode

```cpp
enum class ACMode {
    Unknown = 0,
    Allow,
    Deny
};
```

语义：

```text
Unknown:
    没有找到明确规则。
    在 checkPermission / requestPermission 的最终结果中，默认转为 Deny。

Allow:
    明确允许。

Deny:
    明确拒绝。
```

应用层规则：

```cpp
if (acManager.requestPermission("fab.order.modify") != ACMode::Allow) {
    // reject operation
}
```

---

# 5. Permission Matching

## 5.1 Interface

```cpp
class PermissionMatcher {
public:
    virtual ~PermissionMatcher() = default;

    virtual bool matches(
        const Permission& pattern,
        const Permission& permission
    ) const = 0;

    virtual int specificity(
        const Permission& pattern
    ) const = 0;
};
```

## 5.2 Default Matcher

```cpp
class DefaultPermissionMatcher : public PermissionMatcher {
public:
    bool matches(
        const Permission& pattern,
        const Permission& permission
    ) const override;

    int specificity(
        const Permission& pattern
    ) const override;
};
```

## 5.3 Specificity Rule

权限越具体，优先级越高。

建议规则：

```text
exact token      +10
* token          +1
** token         +0
shorter wildcard lower than longer exact path
```

示例：

```text
file.read        specificity higher than file.*
file.*           specificity higher than file.**
fab.order.modify specificity higher than fab.order.*
```

---

# 6. Identity Model

## 6.1 IdentityRef

`IdentityRef` 是 ACL 中使用的静态身份引用。

```cpp
struct IdentityRef {
    std::string type;
    std::string name;
    Realm realm;

    bool operator==(const IdentityRef& other) const;
};
```

示例：

```cpp
IdentityRef{ "user", "alice" };
IdentityRef{ "user", "alice", Realm{.name = "factory-a"} };
IdentityRef{ "role", "admin" };
IdentityRef{ "device", "office-pc-001" };
IdentityRef{ "service", "sync-agent" };
```

`realm` 在 ACL 匹配时：任一侧为空视为通配（见 `realmSame`）。规则 `user:alice@factory-a` 只匹配该 realm 下的 alice。

推荐内置 identity type：

```text
user
role
group
device
service
node
app
tenant
organization
anonymous
public
system
```

但框架不强制枚举，允许业务扩展。

---

## 6.2 IdentityState

```cpp
enum class IdentityState {
    Unknown = 0,
    Active,
    Expired,
    Revoked,
    LoggedOut
};
```

---

## 6.3 IdentitySource

```cpp
enum class IdentitySource {
    Unknown = 0,

    Direct,     // 用户或服务直接登录得到
    Derived,    // 由其他 identity 派生得到，比如 role
    Auto,       // 自动登录得到，比如 device / anonymous
    System      // 系统内置身份
};
```

---

## 6.4 Identity

`Identity` 是运行时已登录身份。

```cpp
struct Identity {
    std::string type;
    std::string name;
    Realm realm;

    std::string displayName;
    std::string serviceId;

    IdentitySource source = IdentitySource::Unknown;
    IdentityState state = IdentityState::Unknown;

    std::chrono::system_clock::time_point issuedAt {};
    std::optional<std::chrono::system_clock::time_point> expiresAt;

    JsonObject attributes;

    IdentityRef ref() const {
        return IdentityRef{ type, name, realm };
    }

    bool isExpired(
        std::chrono::system_clock::time_point now
    ) const {
        return expiresAt.has_value() && now >= *expiresAt;
    }

    bool isActive(
        std::chrono::system_clock::time_point now
    ) const {
        return state == IdentityState::Active && !isExpired(now);
    }
};
```

说明：

```text
IdentityRef:
    用于 ACL 静态规则。

Identity:
    用于运行时登录状态。
```

---

## 6.5 IdentitySet

`IdentityService::login(...)` 返回 `IdentitySet`。

```cpp
struct IdentitySet {
    std::optional<Identity> primary;
    std::vector<Identity> identities;

    bool empty() const {
        return !primary.has_value() && identities.empty();
    }
};
```

示例：

用户 `alice` 登录后可以返回：

```text
primary:
    user:alice

identities:
    user:alice@factory-a
    role:admin
    role:operator
    organization:factory-a
    tenant:main
```

---

## 6.6 Realm

`Realm` 表示登录/凭证所属的作用域（常见用法：一台设备、一个工厂线、一个租户实例）。

```cpp
struct Realm {
    std::string type;   // global, device, app, ...
    std::string uuid;
    std::string name;
    std::string description;
    std::string image;

    bool empty() const;
    bool hasKey() const;  // type、uuid 或 name 非空
};
```

常见 `type` 取值：

```text
global   — 全局/匿名会话（AnonymousIdentityService）
device   — 物理设备或终端（默认用户登录）
app      — 应用实例 scope
```

匹配语义（三种函数，用途不同）：

```text
realmMatches(stored, hint)     — Credential 查找：hint 为空 → 不过滤，匹配任意 stored realm
realmSame(a, b)                — ACL / IdentityRef：任一侧 realm 为空 → 通配
realmScopedEqual(a, b)         — LoginPolicy 并发槽位：仅 empty 对 empty；同 type 下每个 realm 独立计数
```

同一用户可在多个 realm 同时在线，例如 `user:alice@phone` 与 `user:alice@tablet`；同一 realm 内 `user` 类型默认仍只允许一个活跃主体（`maxActive = 1`）。

---

# 7. Access Control Rule Model

## 7.1 ACRule

```cpp
struct ACRule {
    Permission permission;
    ACMode mode = ACMode::Unknown;
};
```

示例：

```cpp
ACRule{ "file.read", ACMode::Allow };
ACRule{ "file.delete", ACMode::Deny };
ACRule{ "fab.order.*", ACMode::Allow };
```

---

## 7.2 ACEntry

```cpp
struct ACEntry {
    IdentityRef identity;
    ACRule rule;
};
```

JSON 示例：

```json
{
  "identity": {
    "type": "role",
    "name": "operator"
  },
  "rule": {
    "permission": "fab.order.modify",
    "mode": "allow"
  }
}
```

---

## 7.3 ACMatch

内部匹配结果。

```cpp
struct ACMatch {
    ACEntry entry;
    int permissionSpecificity = 0;
    int identitySpecificity = 0;
};
```

---

## 7.4 ACResolvePolicy

冲突解析策略。

```cpp
class ACResolvePolicy {
public:
    virtual ~ACResolvePolicy() = default;

    virtual ACMode resolve(
        const std::vector<ACMatch>& matches
    ) const = 0;
};
```

默认策略：

```text
1. 更具体 permission 优先。
2. 同等 permission specificity 下，Deny 优先。
3. Direct identity 优先于 Derived / Auto / System。
4. 如果仍然冲突，Deny 优先。
5. 没有匹配规则，返回 Unknown。
```

简化后的默认原则：

```text
specificity first,
deny second,
unknown last.
```

---

## 7.5 DefaultACResolvePolicy

```cpp
class DefaultACResolvePolicy : public ACResolvePolicy {
public:
    ACMode resolve(
        const std::vector<ACMatch>& matches
    ) const override;
};
```

---

# 8. ACList

`ACList` 是 ACE 集合。

```cpp
class ACList {
public:
    void add(const ACEntry& entry);
    void remove(const ACEntry& entry);
    void clear();

    const std::vector<ACEntry>& entries() const;

    std::vector<ACRule> rulesOf(
        const IdentityRef& identity
    ) const;

    std::vector<ACMatch> find(
        const IdentityRef& identity,
        const Permission& permission,
        const PermissionMatcher& matcher
    ) const;

    ACMode check(
        const IdentityRef& identity,
        const Permission& permission,
        const PermissionMatcher& matcher,
        const ACResolvePolicy& resolver
    ) const;

    ACMode checkAny(
        const std::vector<Identity>& identities,
        const Permission& permission,
        const PermissionMatcher& matcher,
        const ACResolvePolicy& resolver
    ) const;

private:
    std::vector<ACEntry> m_entries;
};
```

注意：

```text
ACList::check / checkAny 可以返回 Unknown。
但 ACManager::checkPermission / requestPermission 默认把 Unknown 转为 Deny。
```

---

# 8.5 Hint Fields（`*Hint` 字段语义）

名称以 **`Hint`** 结尾的字段表示 **查找/预填用的建议值**，不是已登录身份的权威来源。  
它们在 **Request**（查询侧）与 **Meta**（存储侧）成对出现：**Request 里的 hint 用来筛选；Credential 保存后写入 Meta 的对应字段**。

## 8.5.1 字段对照

| 字段 | 出现位置 | 含义 |
|------|----------|------|
| `nameHint` | `ACRequestOptions`, `LoginRequest` | 期望登录的主体名（用户名等）；UI 预填、无缓存时传给 IdentityService |
| `subjectHint` | `CredentialMeta`, `CredentialRequest` | 凭证绑定的主体名；**保存后**来自表单或登录结果 |
| `serviceHint` | `CredentialMeta`, `CredentialRequest` | 凭证适用的 `IdentityService::id()`，区分同一 subject 的不同登录方式 |
| `realmHint` | `ACRequestOptions`, `LoginRequest`, `CredentialRequest` | 查找/登录时的 realm 筛选；**保存后**写入 `CredentialMeta.realm` |

空 hint 的默认行为：

```text
subjectHint / serviceHint 为空  →  不参与过滤（或与 stored 为空时兼容）
realmHint 为空                  →  realmMatches：匹配任意已存 realm
nameHint 为空                   →  不预填用户名；IdentityService 可能再弹表单
```

`nameHint` 与 `subjectHint` 的区别：

```text
nameHint     — 调用方「这次想登录谁」（CLI --subject、request 参数）
subjectHint  — 凭证文件里「这份 secret 属于谁」（登录成功后写入 meta）

流程：nameHint 预填表单 → 用户确认/修改 → 保存为 subjectHint
若已有缓存 credential，login 优先用 credential.meta.subjectHint，不必再填 nameHint
```

## 8.5.2 举例：两台设备各存一份密码

Alice 在工厂 A 的车间平板、工厂 B 的办公室 PC 各登录一次：

```text
Credential #1:
  meta.subjectHint = "alice"
  meta.serviceHint = "bas.identity.user.password"
  meta.realm.name  = "factory-a"
  secret           = ***

Credential #2:
  meta.subjectHint = "alice"
  meta.serviceHint = "bas.identity.user.password"
  meta.realm.name  = "factory-b"
  secret           = ***
```

启动时恢复全部缓存：

```cpp
ACRequestOptions options;          // realmHint 为空 → 所有 realm
ac.activateCachedCredentials(options);
// → user:alice@factory-a 与 user:alice@factory-b 同时 active
```

只恢复工厂 A：

```cpp
ACRequestOptions options;
options.realmHint.name = "factory-a";
ac.activateCachedCredentials(options);
// → 仅加载 meta.realm 匹配 factory-a 的 credential
```

## 8.5.3 举例：requestPermission 带 hint

应用执行敏感操作，希望用 Alice 在 factory-a 的身份：

```cpp
ACRequestOptions options;
options.nameHint = "alice";
options.realmHint.name = "factory-a";
options.reason = "modify order";

ac.requestPermission("fab.order.modify", options);
```

内部等价于向 `CredentialManager` 发起近似查询：

```text
CredentialRequest:
  subjectHint = "alice"        // 来自 options.nameHint
  realmHint   = factory-a
  serviceHint = <当前尝试的 IdentityService::id()>
```

只有 **subject、realm、service 均匹配** 且未过期的 credential 才会被 `get()` / `load()` 选中。

## 8.5.4 举例：serviceHint 区分登录方式

同一 `alice` 可能同时有：

```text
password  credential  → serviceHint = "bas.identity.user.password"
api-token credential  → serviceHint = "bas.identity.api.token"   （假设存在）
```

`CredentialRequest.serviceHint = "bas.identity.user.password"` 只会命中密码那条，不会误用 token。

## 8.5.5 举例：交互登录写入 meta

控制台表单（`ConsoleLoginInteraction`）提交后：

```text
表单 username = "bob"
表单 realm    = "warehouse-1"
→ cred.meta.subjectHint = "bob"
→ cred.meta.realm.name  = "warehouse-1"
→ cred.meta.serviceHint = LoginRequest 关联的 service.id()
```

下次非交互 `activateCachedCredentials` 用 **meta** 里的值重新 login，无需再次输入。

## 8.5.6 realmHint vs Identity.realm

```text
realmHint  — 请求/查询时「我要哪个 scope 的凭证或登录」
realm      — 已登录 Identity 或已保存 Credential 上的「实际 scope」

显示：user:alice@factory-a  →  type=user, name=alice, realm.name=factory-a
```

---

# 9. Credential Model

## 9.1 CredentialRef

`CredentialRef` 是凭证在 store 内的引用（不含 secret、不含 meta）。

```cpp
struct CredentialRef {
    std::string store;   // 例如 "file", "keychain"
    std::string id;      // store 内唯一 id，例如 "cred-1"

    bool empty() const;
};
```

---

## 9.2 CredentialMeta

元数据用于列表、匹配、过期判断；**`*Hint` 字段保存在这里（存储侧）**。

```cpp
struct CredentialMeta {
    std::string type;           // password, token, api-key, ...

    std::string name;           // 展示名，如 "User Login"
    std::string subjectHint;    // 主体名，如 "alice"
    std::string serviceHint;    // IdentityService::id()
    Realm realm;

    std::optional<std::chrono::system_clock::time_point> expiresAt;
    JsonObject attributes;

    bool isExpired(std::chrono::system_clock::time_point now) const;
};
```

常见 credential type：

```text
password
token
api-key
private-key
public-key
key-pair
client-cert
oauth
cookie
webauthn
```

---

## 9.3 SecretValue

为了避免 secret 被日志泄露，需要抽象封装。

```cpp
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
```

说明：

```text
value() 只应在 IdentityService::login 内部短时间使用。
日志、异常、调试输出必须使用 masked()。
```

---

## 9.4 Credential

```cpp
struct Credential {
    CredentialRef ref;
    CredentialMeta meta;

    std::optional<SecretValue> secret;
    JsonObject publicData;
};
```

示例：

password credential：

```text
meta.type        = "password"
meta.subjectHint = "alice"
meta.serviceHint = "bas.identity.user.password"
meta.realm.name  = "factory-a"
secret           = password
```

api-key credential：

```text
meta.type        = "api-key"
meta.name        = key id
meta.subjectHint = "integration-bot"
meta.serviceHint = "bas.identity.api"
secret           = api key
```

private-key credential：

```text
meta.type     = "private-key"
publicData    = key metadata
secret        = encrypted private key or passphrase
```

---

## 9.5 CredentialRequest

向 `CredentialManager` 查询凭证时的 **筛选条件**（hint 在请求侧）。

```cpp
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
};
```

匹配规则（简化）：

```text
types         — credential.meta.type 必须在列表中（列表空则不过滤）
subjectHint   — 非空且 meta.subjectHint 非空时必须相等
serviceHint   — 非空且 meta.serviceHint 非空时必须相等
realmHint     — realmMatches(meta.realm, realmHint)
未过期        — !meta.isExpired(now)
```

`ACManager` 会把 `options.nameHint` 映射为 `credentialRequest.subjectHint`。

---

## 9.6 UserStore（用户目录，与 Credential 分层）

```text
UserProfile     — 用户资料，不含认证秘密
UserAuthKey     — 服务端验证材料（password-hash / public-key / fingerprint）
UserRecord      — 聚合：profile + roles + keys
UserStore       — 管理 UserRecord

Credential      — 客户端提交或缓存的登录材料（可含 secret）
```

`UserRecord` 不是另一份平行数据，而是 **profile + roles + keys 的容器**。

```cpp
struct UserProfile {
    std::string name;
    std::string displayName;
    std::string email;
    bool enabled = true;
    JsonObject attributes;  // 仅非认证信息
};

struct UserAuthKey {
    std::string id;
    std::string type;       // password-hash, public-key, fingerprint, …
    std::string name;
    bool enabled = true;
    std::optional<std::chrono::system_clock::time_point> createdAt;
    std::optional<std::chrono::system_clock::time_point> expiresAt;
    JsonObject data;        // 验证材料（hash、publicKey 等），非明文 secret
    JsonObject attributes;
};

struct UserRecord {
    UserProfile profile;
    std::vector<std::string> roles;
    std::vector<UserAuthKey> keys;
    JsonObject attributes;  // 记录级元数据，同样不得放 secret
};
```

规则：

```text
UserProfile.attributes / UserRecord.attributes — 部门、电话、备注等
不得存放 password、passwordHash、apiToken、privateKey 等

UserAuthKey.data — 服务端保存的 verification material
Credential.secret — 用户本次登录提交的材料（短期、可缓存）
```

JSON 文件格式：

```json
{
  "version": 1,
  "users": [
    {
      "profile": {
        "name": "alice",
        "displayName": "Alice",
        "enabled": true,
        "attributes": { "department": "factory" }
      },
      "roles": ["operator"],
      "keys": [
        {
          "id": "pwd-main",
          "type": "password-hash",
          "name": "Main Password",
          "enabled": true,
          "data": {
            "algorithm": "argon2id",
            "hash": "...",
            "salt": "..."
          }
        }
      ]
    }
  ]
}
```

实现：`DemoUserStore`、`DefaultUserStore`、`DecoratedUserStore`、`FileUserStore`；各类型实现 `IJsonForm`；`StoreIdentityService` 使用 `UserStore` 校验密码并加载 roles。

---

# 10. CredentialManager

`CredentialManager` 负责缓存、查找、记住各种凭证。

它不负责登录，不负责权限判断。

```cpp
class CredentialManager {
public:
    virtual ~CredentialManager() = default;

    virtual std::optional<Credential> get(
        const CredentialRequest& request
    ) = 0;

    virtual void put(
        Credential credential
    ) = 0;

    virtual void remove(
        const CredentialRef& ref
    ) = 0;

    virtual void clear() = 0;

    virtual std::vector<CredentialRef> list(
        const std::optional<std::string>& type = std::nullopt
    ) const = 0;
};
```

典型实现：

```text
MemoryCredentialManager
FileCredentialManager
KeychainCredentialManager
CompositeCredentialManager
```

推荐组合：

```text
CompositeCredentialManager:
    MemoryCredentialManager
    KeychainCredentialManager
    FileCredentialManager
```

---

# 11. Login Form Model

GUI 登录不写死在 ACManager 中。
`IdentityService` 可以返回 `LoginFormSpec`，由 GUI 层渲染。

## 11.1 LoginField

```cpp
struct LoginField {
    std::string name;
    std::string type;
    std::string label;

    bool required = false;

    JsonObject options;
};
```

常见 field type：

```text
text
password
select
checkbox
key-selector
file
oauth-provider
webauthn
```

---

## 11.2 LoginAction

```cpp
struct LoginAction {
    std::string name;
    std::string label;
    JsonObject options;
};
```

---

## 11.3 LoginFormSpec

```cpp
struct LoginFormSpec {
    std::string title;
    std::vector<LoginField> fields;
    std::vector<LoginAction> actions;
    JsonObject options;
};
```

密码登录示例（含 realm 预填字段）：

```json
{
  "title": "Login",
  "fields": [
    {
      "name": "username",
      "type": "text",
      "label": "Username",
      "required": true
    },
    {
      "name": "password",
      "type": "password",
      "label": "Password",
      "required": true
    },
    {
      "name": "realm",
      "type": "text",
      "label": "Realm",
      "options": { "value": "factory-a" }
    },
    {
      "name": "realmUuid",
      "type": "text",
      "label": "Realm UUID"
    }
  ]
}
```

`realm` / `realmUuid` 的 `options.value` 来自 `LoginRequest.realmHint`；提交后写入 `CredentialMeta.realm`。

---

# 12. Identity Service

## 12.1 LoginStatus

```cpp
enum class LoginStatus {
    Unknown = 0,
    Success,
    NeedCredential,
    NeedInteraction,
    Failed,
    Unsupported
};
```

---

## 12.2 LoginRequest

```cpp
struct LoginRequest {
    std::string identityType;
    std::string nameHint;
    Realm realmHint;

    Permission permission;

    std::optional<Credential> credential;

    bool interactive = false;

    JsonObject context;
};
```

`nameHint` / `realmHint` 用于预填交互表单；若 `credential` 已带 `meta.subjectHint` / `meta.realm`，`IdentityService` 优先使用 credential 中的值。

---

## 12.3 LoginResult

```cpp
struct LoginResult {
    LoginStatus status = LoginStatus::Unknown;

    std::optional<IdentitySet> identities;

    std::vector<std::string> requiredCredentialTypes;

    std::optional<LoginFormSpec> form;

    std::string error;
};
```

---

## 12.4 IdentityService Interface

```cpp
class IdentityService {
public:
    virtual ~IdentityService() = default;

    virtual std::string id() const = 0;

    virtual std::string identityType() const = 0;

    virtual std::vector<std::string> supportedCredentialTypes() const = 0;

    virtual bool canAutoLogin() const {
        return false;
    }

    virtual LoginResult login(
        const LoginRequest& request
    ) = 0;

    virtual void logout(
        const Identity& identity
    ) = 0;

    virtual std::optional<IdentitySet> refresh(
        const Identity& identity
    ) {
        return std::nullopt;
    }
};
```

---

## 12.5 Login Semantics

`login(...) -> IdentitySet`。

一个 login 不一定只返回一个 identity。

例如：

```text
PasswordUserIdentityService.login(password alice)
    returns:
        primary = user:alice
        identities =
            user:alice
            role:admin
            role:operator
            organization:factory-a
```

自动登录示例：

```text
DeviceIdentityService.login(auto)
    returns:
        primary = device:office-pc-001
        identities =
            device:office-pc-001
```

匿名登录示例：

```text
AnonymousIdentityService.login(auto)
    returns:
        primary = anonymous:default
        identities =
            anonymous:default
            public:default
```

---

# 13. IdentityRegistry

`IdentityRegistry` 管理所有 `IdentityService`。

```cpp
class IdentityRegistry {
public:
    void add(
        std::shared_ptr<IdentityService> service
    );

    void remove(
        const std::string& serviceId
    );

    std::vector<std::shared_ptr<IdentityService>> list() const;

    std::vector<std::shared_ptr<IdentityService>> findByIdentityType(
        const std::string& type
    ) const;

    std::vector<std::shared_ptr<IdentityService>> findByCredentialType(
        const std::string& credentialType
    ) const;

    std::vector<std::shared_ptr<IdentityService>> findAutoLoginServices() const;

private:
    std::vector<std::shared_ptr<IdentityService>> m_services;
};
```

---

# 14. Identity Login Policy

ACManager 需要管理哪些 identity 可以同时登录。

通常（**按 `identityType + realm` 计数**，见 `realmScopedEqual`）：

```text
user:
    每个 realm 同时只能有一个 user
    例：user:alice@factory-a 与 user:bob@factory-b 可并存

role:
    可以有多个（各 realm 独立计数）

device:
    每个 realm 通常只有一个

service:
    可以有多个

app:
    每个 realm 通常只有一个

anonymous / public:
    每个 realm 通常各一份
```

`LoginPolicy::canCoexist` 只在 **相同 type 且 realmScopedEqual** 时计入冲突；不同 realm 的同名 type 互不影响。

---

## 14.1 LoginConflictAction

```cpp
enum class LoginConflictAction {
    KeepExisting = 0,
    ReplaceExisting,
    RejectIncoming,
    AskUser
};
```

---

## 14.2 IdentityConcurrencyRule

```cpp
struct IdentityConcurrencyRule {
    std::string identityType;

    std::size_t maxActive = 1;

    LoginConflictAction conflictAction =
        LoginConflictAction::ReplaceExisting;
};
```

---

## 14.3 LoginPolicy

```cpp
class LoginPolicy {
public:
    void setRule(
        IdentityConcurrencyRule rule
    );

    IdentityConcurrencyRule ruleOf(
        const std::string& identityType
    ) const;

    bool canCoexist(
        const std::vector<Identity>& active,
        const Identity& incoming
    ) const;

    LoginConflictAction resolveConflict(
        const std::vector<Identity>& active,
        const Identity& incoming
    ) const;

private:
    std::unordered_map<std::string, IdentityConcurrencyRule> m_rules;
};
```

默认规则：

```text
user:
    maxActive = 1
    conflictAction = ReplaceExisting

role:
    maxActive = unlimited
    conflictAction = KeepExisting

group:
    maxActive = unlimited
    conflictAction = KeepExisting

organization:
    maxActive = unlimited
    conflictAction = KeepExisting

tenant:
    maxActive = 1
    conflictAction = ReplaceExisting

device:
    maxActive = 1
    conflictAction = KeepExisting

app:
    maxActive = 1
    conflictAction = KeepExisting

service:
    maxActive = unlimited
    conflictAction = KeepExisting

anonymous:
    maxActive = 1
    conflictAction = KeepExisting

public:
    maxActive = 1
    conflictAction = KeepExisting
```

---

# 15. ACManager

`ACManager` 是应用侧请求权限的主入口。

它管理：

```text
1. ACList
2. 当前 active identities
3. CredentialManager
4. IdentityRegistry
5. LoginPolicy
6. PermissionMatcher
7. ACResolvePolicy
```

---

## 15.1 ACRequestOptions

```cpp
struct ACRequestOptions {
    bool interactive = true;
    bool allowAutoLogin = true;

    std::vector<std::string> preferredIdentityTypes;

    std::string nameHint;
    Realm realmHint;
    std::string reason;

    JsonObject context;
};
```

`nameHint` → 传给 `CredentialRequest.subjectHint` 与 `LoginRequest.nameHint`。  
`realmHint` → 限制 credential 查找、IdentityRegistry 筛选、以及 `activateCachedCredentials` 的 realm 范围（空 = 全部 realm）。

---

## 15.2 ACManager Interface

```cpp
class ACManager {
public:
    ACManager(
        std::shared_ptr<ACList> acl,
        std::shared_ptr<CredentialManager> credentialManager,
        std::shared_ptr<IdentityRegistry> identityRegistry,
        std::shared_ptr<LoginPolicy> loginPolicy,
        std::shared_ptr<PermissionMatcher> permissionMatcher,
        std::shared_ptr<ACResolvePolicy> resolvePolicy
    );

    void start();

    std::vector<Identity> currentIdentities() const;

    void activate(
        const IdentitySet& identitySet
    );

    void logout(
        const IdentityRef& identity
    );

    void logoutType(
        const std::string& identityType
    );

    void logoutAll();

    bool autoLoginSuppressed() const;

    ACMode checkPermission(
        const Permission& permission
    ) const;

    ACMode requestPermission(
        const Permission& permission,
        const ACRequestOptions& options = {}
    );

    void requirePermission(
        const Permission& permission,
        const ACRequestOptions& options = {}
    );

    /** 从 CredentialManager 批量非交互登录；realmHint 空则恢复所有 realm。 */
    std::size_t activateCachedCredentials(
        const ACRequestOptions& options = {}
    );

    void logoutRealm(
        const Realm& realm
    );

    void setInteractionProvider(
        std::shared_ptr<LoginInteractionProvider> provider
    );

private:
    ACMode checkPermissionRaw(
        const Permission& permission
    ) const;

    ACMode normalizeResult(
        ACMode mode
    ) const;

    void tryAutoLogin(
        const Permission& permission,
        const ACRequestOptions& options
    );

    std::vector<std::shared_ptr<IdentityService>> selectServices(
        const Permission& permission,
        const ACRequestOptions& options
    ) const;

    bool tryLoginWithService(
        IdentityService& service,
        const Permission& permission,
        const ACRequestOptions& options
    );

    void activateOne(
        const Identity& identity
    );

private:
    std::shared_ptr<ACList> m_acl;
    std::shared_ptr<CredentialManager> m_credentialManager;
    std::shared_ptr<IdentityRegistry> m_identityRegistry;
    std::shared_ptr<LoginPolicy> m_loginPolicy;
    std::shared_ptr<PermissionMatcher> m_permissionMatcher;
    std::shared_ptr<ACResolvePolicy> m_resolvePolicy;

    std::vector<Identity> m_activeIdentities;
};
```

---

# 16. ACManager Permission Semantics

## 16.1 checkPermission

```cpp
ACMode checkPermission(const Permission& permission) const;
```

语义：

```text
1. 只检查当前 active identities。
2. 不触发自动登录。
3. 不弹 GUI。
4. 不请求 CredentialManager。
5. 不改变 ACManager 状态。
6. Unknown 默认转成 Deny。
```

也就是说：

```text
checkPermission(...) 只回答：当前登录状态下是否允许？
```

默认：

```text
Unknown -> Deny
```

---

## 16.2 requestPermission

```cpp
ACMode requestPermission(
    const Permission& permission,
    const ACRequestOptions& options = {}
);
```

语义：

```text
1. 先检查当前 active identities。
2. 如果已有 Allow / Deny，直接返回。
3. 如果 Unknown，并且 allowAutoLogin=true，尝试自动登录。
4. 自动登录后再次检查。
5. 如果仍然 Unknown，查询 IdentityService。
6. 根据 IdentityService.supportedCredentialTypes() 向 CredentialManager 索要凭证。
7. 如果凭证可用，尝试 login。
8. 如果需要交互并且 options.interactive=true，返回 LoginFormSpec 给 GUI 层或触发 GUI provider。
9. 登录成功后 activate IdentitySet。
10. 再次检查 ACL。
11. Unknown 默认转成 Deny。
```

默认：

```text
Unknown -> Deny
```

---

## 16.3 requirePermission

```cpp
void requirePermission(
    const Permission& permission,
    const ACRequestOptions& options = {}
);
```

语义：

```text
requestPermission(permission, options) != Allow 时抛出 AccessDenied。
```

示例：

```cpp
acManager.requirePermission("fab.order.modify");
modifyOrder();
```

---

# 17. AccessDenied

```cpp
class AccessDenied : public std::runtime_error {
public:
    explicit AccessDenied(
        const Permission& permission
    )
        : std::runtime_error("access denied: " + permission),
          m_permission(permission) {}

    const Permission& permission() const {
        return m_permission;
    }

private:
    Permission m_permission;
};
```

---

# 18. requestPermission Recommended Algorithm

```cpp
ACMode ACManager::requestPermission(
    const Permission& permission,
    const ACRequestOptions& options
) {
    // 1. Check current identities.
    ACMode mode = checkPermissionRaw(permission);

    if (mode != ACMode::Unknown) {
        return normalizeResult(mode);
    }

    // 2. Try auto login.
    if (options.allowAutoLogin) {
        tryAutoLogin(permission, options);

        mode = checkPermissionRaw(permission);

        if (mode != ACMode::Unknown) {
            return normalizeResult(mode);
        }
    }

    // 3. Select identity services.
    auto services = selectServices(permission, options);

    // 4. Try login through each service.
    for (auto& service : services) {
        bool changed = tryLoginWithService(
            *service,
            permission,
            options
        );

        if (!changed) {
            continue;
        }

        mode = checkPermissionRaw(permission);

        if (mode != ACMode::Unknown) {
            return normalizeResult(mode);
        }
    }

    // 5. Default deny.
    return ACMode::Deny;
}
```

---

# 19. checkPermission Recommended Algorithm

```cpp
ACMode ACManager::checkPermission(
    const Permission& permission
) const {
    return normalizeResult(
        checkPermissionRaw(permission)
    );
}
```

```cpp
ACMode ACManager::checkPermissionRaw(
    const Permission& permission
) const {
    return m_acl->checkAny(
        m_activeIdentities,
        permission,
        *m_permissionMatcher,
        *m_resolvePolicy
    );
}
```

```cpp
ACMode ACManager::normalizeResult(
    ACMode mode
) const {
    if (mode == ACMode::Unknown) {
        return ACMode::Deny;
    }

    return mode;
}
```

---

# 20. tryLoginWithService Algorithm

```cpp
bool ACManager::tryLoginWithService(
    IdentityService& service,
    const Permission& permission,
    const ACRequestOptions& options
) {
    auto credentialTypes = service.supportedCredentialTypes();

    CredentialRequest credentialRequest;
    credentialRequest.types = credentialTypes;
    credentialRequest.identityType = service.identityType();
    credentialRequest.subjectHint = options.nameHint;
    credentialRequest.serviceHint = service.id();
    credentialRequest.realmHint = options.realmHint;
    credentialRequest.permission = permission;
    credentialRequest.interactive = false;
    credentialRequest.reason = options.reason;
    credentialRequest.context = options.context;

    auto credential = m_credentialManager->get(credentialRequest);

    LoginRequest loginRequest;
    loginRequest.identityType = service.identityType();
    loginRequest.nameHint = options.nameHint;
    loginRequest.realmHint = options.realmHint;
    loginRequest.credential = credential;
    loginRequest.interactive = false;
    loginRequest.context = options.context;

    LoginResult result = service.login(loginRequest);

    if (result.status == LoginStatus::Success && result.identities.has_value()) {
        activate(*result.identities);
        return true;
    }

    if (
        result.status == LoginStatus::NeedInteraction &&
        options.interactive
    ) {
        // GUI handling is intentionally outside core.
        // A concrete ACManager may delegate to LoginInteractionProvider.
        return false;
    }

    return false;
}
```

---

# 21. Optional LoginInteractionProvider

如果你希望 core package 不依赖 GUI，但又允许 ACManager 触发交互，可以增加一个可选接口。

```cpp
class LoginInteractionProvider {
public:
    virtual ~LoginInteractionProvider() = default;

    virtual std::optional<Credential> requestCredential(
        const LoginFormSpec& form,
        const CredentialRequest& request
    ) = 0;
};
```

然后 ACManager 可选持有：

```cpp
std::shared_ptr<LoginInteractionProvider> m_interactionProvider;
```

这样 GUI / CLI / Web 都可以独立实现：

```text
GuiLoginInteractionProvider
CliLoginInteractionProvider
WebLoginInteractionProvider
NullLoginInteractionProvider
```

没有 provider 时：

```text
NeedInteraction -> login failed silently -> requestPermission returns Deny
```

---

# 22. activate IdentitySet

```cpp
void ACManager::activate(
    const IdentitySet& identitySet
) {
    if (identitySet.primary.has_value()) {
        activateOne(*identitySet.primary);
    }

    for (const auto& identity : identitySet.identities) {
        activateOne(identity);
    }
}
```

`activateOne` 需要遵守 `LoginPolicy`。

```cpp
void ACManager::activateOne(
    const Identity& incoming
) {
    auto action = m_loginPolicy->resolveConflict(
        m_activeIdentities,
        incoming
    );

    switch (action) {
    case LoginConflictAction::KeepExisting:
        if (m_loginPolicy->canCoexist(activeIdentities_, incoming)) {
            activeIdentities_.push_back(incoming);
        }
        break;

    case LoginConflictAction::ReplaceExisting:
        erase all conflicting active identities;
        m_activeIdentities.push_back(incoming);
        break;

    case LoginConflictAction::RejectIncoming:
        break;

    case LoginConflictAction::AskUser:
        // core may fallback to RejectIncoming or ReplaceExisting
        // depending on configuration.
        break;
    }
}
```

建议默认：

```text
AskUser 在无 GUI provider 时等同 RejectIncoming。
```

---

# 23. Auto Login

## 23.1 start()

```cpp
void ACManager::start() {
    ACRequestOptions options;
    options.interactive = false;
    options.allowAutoLogin = true;
    options.reason = "ACManager startup";

    tryAutoLogin("", options);
}
```

---

## 23.2 tryAutoLogin

```cpp
void ACManager::tryAutoLogin(
    const Permission& permission,
    const ACRequestOptions& options
) {
    auto services = m_identityRegistry->findAutoLoginServices();

    for (auto& service : services) {
        LoginRequest request;
        request.identityType = service->identityType();
        request.permission = permission;
        request.interactive = false;
        request.context = options.context;

        LoginResult result = service->login(request);

        if (
            result.status == LoginStatus::Success &&
            result.identities.has_value()
        ) {
            activate(*result.identities);
        }
    }
}
```

自动登录适合：

```text
anonymous
public
device
node
app
service-account
system
```

---

## 23.3 activateCachedCredentials（多 realm / 多设备）

应用启动或 `reload-creds` 时，可一次性恢复所有已缓存登录：

```cpp
ACRequestOptions options;
options.interactive = false;
options.reason = "restore cached logins";
// options.realmHint 留空 → 每个匹配的 credential 各 login 一次

std::size_t n = ac.activateCachedCredentials(options);
```

对每个 `IdentityService`，`list` + `load` 所有匹配的 credential，用 `meta.subjectHint` / `meta.realm` 构造 `LoginRequest` 并 `activate`。

`logoutAll()` 会设置 `autoLoginSuppressed`，阻止 `requestPermission` 路径上的匿名自动登录；**显式**调用 `activateCachedCredentials` 不受此限制（便于 `reload-creds`）。

`logoutRealm(realm)` 只移除该 realm 下的 active identities，其它设备/session 保持登录。

---

# 24. Example Identity Services

## 24.1 AnonymousIdentityService

```cpp
class AnonymousIdentityService : public IdentityService {
public:
    std::string id() const override {
        return "bas.identity.anonymous";
    }

    std::string identityType() const override {
        return "anonymous";
    }

    std::vector<std::string> supportedCredentialTypes() const override {
        return {};
    }

    bool canAutoLogin() const override {
        return true;
    }

    LoginResult login(
        const LoginRequest& request
    ) override {
        Identity anonymous;
        anonymous.type = "anonymous";
        anonymous.name = "default";
        anonymous.displayName = "Anonymous";
        anonymous.source = IdentitySource::Auto;
        anonymous.state = IdentityState::Active;
        anonymous.issuedAt = std::chrono::system_clock::now();

        Identity publicIdentity;
        publicIdentity.type = "public";
        publicIdentity.name = "default";
        publicIdentity.displayName = "Public";
        publicIdentity.source = IdentitySource::Auto;
        publicIdentity.state = IdentityState::Active;
        publicIdentity.issuedAt = anonymous.issuedAt;

        IdentitySet set;
        set.primary = anonymous;
        set.identities.push_back(anonymous);
        set.identities.push_back(publicIdentity);

        return LoginResult{
            .status = LoginStatus::Success,
            .identities = set
        };
    }

    void logout(
        const Identity& identity
    ) override {
    }
};
```

---

## 24.2 StoreIdentityService

```cpp
class StoreIdentityService : public IdentityService {
public:
    std::string id() const override {
        return "bas.identity.user.password";
    }

    std::string identityType() const override {
        return "user";
    }

    std::vector<std::string> supportedCredentialTypes() const override {
        return { "password" };
    }

    LoginResult login(
        const LoginRequest& request
    ) override {
        if (!request.credential.has_value()) {
            LoginFormSpec form;
            form.title = "User Login";
            form.fields.push_back(LoginField{
                .name = "username",
                .type = "text",
                .label = "Username",
                .required = true
            });
            form.fields.push_back(LoginField{
                .name = "password",
                .type = "password",
                .label = "Password",
                .required = true
            });

            return LoginResult{
                .status = LoginStatus::NeedInteraction,
                .requiredCredentialTypes = { "password" },
                .form = form
            };
        }

        const Credential& credential = *request.credential;

        if (credential.ref.type != "password") {
            return LoginResult{
                .status = LoginStatus::Unsupported,
                .error = "unsupported credential type"
            };
        }

        // authenticate username/password here

        Identity user;
        user.type = "user";
        user.name = credential.ref.subjectHint;
        user.displayName = credential.ref.subjectHint;
        user.source = IdentitySource::Direct;
        user.state = IdentityState::Active;
        user.issuedAt = std::chrono::system_clock::now();

        Identity role;
        role.type = "role";
        role.name = "operator";
        role.source = IdentitySource::Derived;
        role.state = IdentityState::Active;
        role.issuedAt = user.issuedAt;

        IdentitySet set;
        set.primary = user;
        set.identities.push_back(user);
        set.identities.push_back(role);

        return LoginResult{
            .status = LoginStatus::Success,
            .identities = set
        };
    }

    void logout(
        const Identity& identity
    ) override {
    }
};
```

---

# 25. Application Operation Model

应用操作可以声明 permission。

```cpp
struct OperationRequest {
    std::string name;
    Permission permission;
    JsonObject parameters;
};
```

示例：

```cpp
OperationRequest request {
    .name = "modify order",
    .permission = "fab.order.modify"
};
```

执行：

```cpp
if (acManager.requestPermission(request.permission) == ACMode::Allow) {
    executeOperation(request);
} else {
    rejectOperation(request);
}
```

或者：

```cpp
acManager.requirePermission("fab.order.modify");
executeOperation(request);
```

---

# 26. JSON Serialization

需要支持 `IJsonForm` 的类型：

```text
IdentityRef
ACRule
ACEntry
ACList
CredentialRef
LoginFormSpec
LoginField
LoginAction
Identity
IdentitySet
```

推荐 JSON 表达：

## 26.1 ACEntry

```json
{
  "identity": {
    "type": "role",
    "name": "admin"
  },
  "rule": {
    "permission": "file.*",
    "mode": "allow"
  }
}
```

## 26.2 ACList

```json
{
  "entries": [
    {
      "identity": {
        "type": "role",
        "name": "admin"
      },
      "rule": {
        "permission": "file.*",
        "mode": "allow"
      }
    },
    {
      "identity": {
        "type": "user",
        "name": "bob"
      },
      "rule": {
        "permission": "file.delete",
        "mode": "deny"
      }
    }
  ]
}
```

## 26.3 Identity

```json
{
  "type": "user",
  "name": "alice",
  "realm": {
    "name": "factory-a",
    "uuid": "550e8400-e29b-41d4-a716-446655440000"
  },
  "displayName": "Alice",
  "serviceId": "bas.identity.user.password",
  "source": "direct",
  "state": "active",
  "attributes": {
    "department": "factory"
  }
}
```

## 26.4 Credential 文件（FileCredentialManager）

```json
{
  "credentials": [
    {
      "store": "file",
      "id": "cred-1",
      "type": "password",
      "name": "User Login",
      "subjectHint": "alice",
      "serviceHint": "bas.identity.user.password",
      "realm": { "name": "factory-a" },
      "secret": "..."
    }
  ]
}
```

也兼容顶层为 JSON array 的旧格式（仅 `credentials` 数组）。

---

# 27. Security Rules

## 27.1 Default Deny

所有面向应用的权限检查默认 deny：

```text
checkPermission:
    Unknown -> Deny

requestPermission:
    Unknown -> Deny

requirePermission:
    not Allow -> throw AccessDenied
```

底层 `ACList::check` 可以保留 `Unknown`，用于表达“无匹配规则”。

---

## 27.2 Credential Isolation

```text
CredentialManager 只管理凭证。
IdentityService 只负责登录。
ACList 只负责授权规则。
ACManager 负责调度。
```

禁止：

```text
ACList 读取 password/token。
CredentialManager 直接判断 permission。
IdentityService 修改 ACList。
业务 operation 直接访问 Credential secret。
```

---

## 27.3 Secret Logging

```text
SecretValue 不得被默认输出。
Credential 不得直接序列化 secret。
异常信息不得包含 secret。
日志只能使用 masked()。
```

---

## 27.4 GUI Independence

核心包不依赖 GUI 框架。

```text
IdentityService 返回 LoginFormSpec。
GUI / CLI / Web 层负责渲染 LoginFormSpec。
```

---

# 28. Minimal Header Layout

建议文件结构：

```text
bas/security/permission.hpp
bas/security/identity.hpp
bas/security/credential.hpp
bas/security/ac_rule.hpp
bas/security/ac_list.hpp
bas/security/identity_service.hpp
bas/security/identity_registry.hpp
bas/security/login_policy.hpp
bas/security/ac_manager.hpp
bas/security/json.hpp
```

---

# 29. Main Class Summary

```text
PermissionMatcher
    匹配 qualified-token permission。

ACRule
    一条 allow / deny rule。

ACEntry
    identity + rule。

ACList
    ACE 集合，负责底层授权匹配。

IdentityRef
    ACL 中的静态身份引用。

Identity
    运行时登录身份。

IdentitySet
    一次登录返回的一组身份。

CredentialRef
    凭证引用。

Credential
    凭证实体，可能包含 secret。

CredentialManager
    凭证缓存和查找。

IdentityService
    登录服务，声明支持的 credential type，并 login(...) -> IdentitySet。

IdentityRegistry
    IdentityService 注册表。

LoginPolicy
    控制多个 identity 是否可以同时激活。

LoginFormSpec
    登录表单抽象。

ACManager
    应用请求权限的主入口。
```

---

# 30. Canonical Usage

```cpp
using namespace bas::security;

auto acl = std::make_shared<ACList>();

acl->add(ACEntry{
    .identity = IdentityRef{ "role", "operator" },
    .rule = ACRule{ "fab.order.modify", ACMode::Allow }
});

acl->add(ACEntry{
    .identity = IdentityRef{ "user", "bob" },
    .rule = ACRule{ "fab.order.delete", ACMode::Deny }
});

auto credentialManager = std::make_shared<MemoryCredentialManager>();

auto registry = std::make_shared<IdentityRegistry>();
registry->add(std::make_shared<AnonymousIdentityService>());
registry->add(std::make_shared<StoreIdentityService>(userStore));

auto loginPolicy = std::make_shared<LoginPolicy>();
auto matcher = std::make_shared<DefaultPermissionMatcher>();
auto resolver = std::make_shared<DefaultACResolvePolicy>();

ACManager acManager(
    acl,
    credentialManager,
    registry,
    loginPolicy,
    matcher,
    resolver
);

acManager.start();

if (acManager.requestPermission("fab.order.modify") == ACMode::Allow) {
    modifyOrder();
} else {
    reject();
}
```

---

# 31. Design Conclusion

`bas.security` 的核心抽象最终收敛为：

```text
permission:
    qualified-token

subject:
    IdentityRef(type, name)

authorization:
    ACEntry(identity, rule)

authentication:
    IdentityService.login(...) -> IdentitySet

credential memory:
    CredentialManager

runtime authority:
    ACManager.activeIdentities

application entry:
    ACManager.requestPermission(permission)
```

最重要的工程边界是：

```text
CredentialManager 记住凭证；
IdentityService 负责登录；
IdentityRegistry 管理登录服务；
ACList 保存授权规则；
ACManager 连接所有部件，并维护当前登录 identities。
```
