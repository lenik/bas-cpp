# 安全 / 访问控制架构

命名空间：`bas::security`  
代码：`src/bas/security/`  
演示：`app/acdemo.cpp`、`app/tanks_rbac.*`、`app/tanks_game.*`  
测试：`tests/bas/security/security_test.cpp`

英文版：[SecurityArch.md](SecurityArch.md)

本文描述 bas-cpp 实现的访问控制模型：身份、领域（realm）、策略存储、会话生命周期，以及应用如何组装这些组件。

---

## 1. 目标

- 分离 **认证**（你是谁）与 **授权**（你能做什么）。
- 在同一进程中支持 **多个领域**（设备、租户、应用），各自独立登录会话。
- 用 **命名 ACL**、**绑定（binding）** 与 **直接授权（grant）** 表达策略。
- 支持仅检查、不弹窗的 **check**，以及可能交互登录的 **request**。
- 支持 **一次性提权（one-shot elevation）**：校验另一主体权限而不替换当前会话。

**没有** 并发/登录策略表（LoginPolicy）。会话替换由 **身份来源**（`IdentitySource::Login`）与领域范围驱动。

---

## 2. 概念

### Realm（领域）

领域表示租户 / 设备 / 应用作用域（`Realm`：`type`、`uuid`、`name` 等）。

| 方法 | 含义 |
|------|------|
| `match(hint)` | 过滤：hint 中空字段为通配。 |
| `same(other)` | 身份/ACL 相等：任一侧为空则为通配。 |
| `scopedEqual(other)` | 登录会话槽位严格相等（空 ≡ 空）。 |
| `storageKey()` | 稳定映射键（`uuid:…` / `type:name` / …）。 |

`Realm::GLOBAL` 是单存储演示的默认共享领域。

### Identity（身份）

领域中的类型化主体：

```text
IdentityRef = { type, realm, name }   // 例：user:alice @ device:tank-a
Identity    = IdentityRef + displayName, serviceId, source, state, times, attributes
IdentitySet = 可选 primary + identities 向量
```

常见类型：`user`、`role`、`anonymous`、`public`、…

**来源**（`IdentitySource`）标记出处并驱动会话清理：

| 来源 | 典型用途 | 会被 `logoutRealm` / 切换登录清除？ |
|------|----------|--------------------------------------|
| `Login` | 密码登录（`StoreIdentityService`） | 是 |
| `Auto` | 匿名 / 公开自动登录 | 否 |
| `Derived` / `Direct` / `System` | 手动 activate、系统主体 | 否（除非匹配同一 `IdentityRef`） |

辅助函数：`isLoginSessionIdentity(id)` ⟺ `source == Login`。

### Subject（主体）

`Subject` 是授权时看到的一组身份（由 `IdentitySet` 展平）。检查既可针对 **当前会话** 主体，也可针对 **显式** 主体（一次性提权）。

### Permission（权限）

`Permission` 含可选的 `action` 与 `resource`。**空字段表示全部。**

规范文本形式（以 `;` 分隔的键值对；值可加引号）：

```text
action=<action>;resource=<resource>
```

示例：

- `action=fire;resource=device` — 设备上的开火
- `resource=device` — 设备上的任意 action
- `action=view;resource=fab.order` — 查看 fab.order
- `action=read;resource="file;special"` — 值中含 `;` 时加引号
- _（空）_ — 匹配一切

action 通配（按点分段）：`*` 一个段，`**` 零个或多个。  
`parse()` 仍接受无 `=` 的旧式简写（`file.read` 或 `file.read:res`）。

特异度：字面量 > `*` > `**`；用于裁决冲突 ACE。

### Access effect（访问效果）

`AccessEffect`：`Unknown` | `Allow` | `Deny`。

- 策略查找可能返回 **Unknown**（无决定性 ACE）。
- 公开的 `checkPermission*` 会把 **Unknown → Deny**。
- `requestPermission` 将 Unknown 视为「继续尝试」（自动登录 / 交互登录）。

---

## 3. 组件总览

```text
                         LoginUi（可选）
                              |
  CredentialManager <--- SecurityManager ---> PolicyStore（+ RealmPolicyStore）
                              |
                       IdentityRegistry
                     +--------+--------+
                     |        |        |
               AnonymousIS  StoreIS  StoreIS
                            @realmA  @realmB
                                |
                            UserStore
```

| 组件 | 职责 |
|------|------|
| **SecurityManager** | 活动会话、登录/登出、权限检查、可选交互提权。 |
| **PolicyStore** | ACList、binding、grant；`effectiveEntriesOf(IdentityRef)`。 |
| **RealmPolicyStore** | 按身份/领域将策略操作路由到各领域存储（+ 全局）。 |
| **IdentityRegistry** | 领域槽位 → `IdentityService`；发现可自动登录的服务。 |
| **IdentityService** | 认证 → `IdentitySet`。 |
| **UserStore** | 持久化用户记录（不是会话状态）。 |
| **CredentialManager** | 客户端凭证缓存（登录用密钥）。 |
| **LoginUi** | 登录需要交互时采集凭证。 |
| **PermissionMatcher** / **ACResolvePolicy** | 匹配权限；在 ACE 匹配中裁决 Allow/Deny。 |

---

## 4. 策略模型

策略分三层（均可选）：

1. **ACList** — 命名的 `{permission, effect}` 列表，**不含**身份。
2. **PolicyBinding** — 将 ACList id 绑定到 `IdentityRef`（通常是角色）。
3. **AccessGrant** — 直接 `{identity, permission, effect}`。

某身份的有效 ACE：

```text
effectiveEntriesOf(id) = grantsOf(id) ∪ 所有绑定到 id 的 ACL 条目
```

### 裁决（`DefaultACResolvePolicy`）

在匹配的 ACE 中：

1. 优先最高 **权限特异度**。
2. 同特异度下 **Deny 优先于 Allow**。
3. 多个 Allow 时，优先更高 **身份特异度**（`10 + IdentitySource::priority()`）。

`DefaultAccessDecisionResolver`（供 `evaluate()` 使用）对原始 `ACEntry` 列表采用类似的 deny-wins 规则。

### RealmPolicyStore

- `addRealmStore(realm, store)` / `setGlobalStore(store)`。
- grants/bindings/`effectiveEntriesOf` 经 `identity.realm` → 对应存储，否则全局。
- 按 ACL id 查询时扫描各领域存储再全局；`addAcl` 默认写入全局（除非上层另行路由）。

双设备演示据此在同一个 `SecurityManager` 下隔离 **tank-a** 与 **tank-b** 的 ACL。

---

## 5. 会话生命周期

会话状态仅存在于 `SecurityManager`：

- `m_activeIdentities` — 当前活动主体
- `m_suppressAutoLogin` — 登出时置位；成功交互登录后清除

### activate(IdentitySet)

1. 收集传入 **Login** 身份所属领域。
2. 对每个领域调用 `clearLoginSession(realm)`（丢弃该领域先前 Login 身份）。
3. 写入新身份（primary 优先；按 `IdentityRef` 去重）。

含义：在同一设备上切换用户会 **替换** 该领域的登录会话（用户 + 角色）。其他领域与 Auto 身份保留。

### logout*

| API | 效果 |
|-----|------|
| `logout(ref)` | 移除一个身份；抑制自动登录。 |
| `logoutType(type)` | 移除该类型全部身份。 |
| `logoutRealm(realm)` | 清除 `scopedEqual(realm)` 的 **Login** 身份。 |
| `logoutAll()` | 清空全部；抑制自动登录。 |

### login 与 authenticate

| API | 是否改会话？ | 用途 |
|-----|--------------|------|
| `login(options)` | 是（`activate`） | 交互 / 缓存凭证登录。 |
| `authenticate(cred, options)` | **否** | 校验凭证 → `IdentitySet`，供一次性检查。 |
| `activateCachedCredentials` | 是 | 启动 / 登录时重放已存凭证。 |

### 权限 API

| API | 行为 |
|-----|------|
| `checkPermission` | 相对活动会话（可选 `realmHint` 过滤）。永不弹窗。Unknown→Deny。 |
| `checkSubjectPermission` | 相对显式 `Subject`。永不弹窗。不改会话。 |
| `requestPermission` | Unknown 时可自动登录和/或交互 LoginUi，再复查。 |
| `requirePermission` | 调用 `requestPermission`；非 Allow 则抛 `AccessDenied`。 |
| `evaluate` | 经 `AccessDecisionResolver` 得到结构化 `AccessDecision`（无登录副作用）。 |

### start()

仅非交互自动登录（如 `AnonymousIdentityService`）。不弹窗。

---

## 6. 身份服务

### AnonymousIdentityService

- `canAutoLogin() == true`
- 发出 `anonymous:default` 与 `public:default`（`IdentitySource::Auto`）
- 用于访客可见授权（如演示中的 `action=view;resource=fab.order`）

### StoreIdentityService

- 由 `UserStore` 支撑
- 密码凭证 → 校验哈希 → 发出：
  - `user:<name>`（`IdentitySource::Login`）
  - 每个角色一条 `role:<role>`（`IdentitySource::Login`）
- 领域来自凭证 / 请求 hint

### 扩展点

实现 `IdentityService`（`login` / `logout` / 可选 `refresh`），经 `IdentityRegistry::add(service, realm?)` 注册。无领域槽位的服务为全局辅助（如匿名）；绑定服务拥有领域槽位（`scopedEqual`）。

---

## 7. 凭证与用户认证密钥

| 存储 | 内容 | 作用 |
|------|------|------|
| **CredentialManager** | 客户端持有的密钥（密码等）+ 提示 | 登录时用于认证 |
| **UserStore keys** | 服务端校验材料（`password-hash` 等） | `StoreIdentityService` 用来校验 |

二者刻意分离。基于文件的演示存储会把哈希/密钥放进 JSON，**不是** 生产级密钥库。

---

## 8. 应用组装

### acdemo — 单存储 REPL

- 一个 `PolicyStore`（演示或文件）+ 一个 `UserStore`
- Registry：Anonymous（未绑定）+ `GLOBAL` 上的 `StoreIdentityService`（及可选 CLI 默认领域）
- `ConsoleLogin` 做交互提示
- 命令：`login`、`whoami`、`check`、`request`、策略/用户 CRUD

### tanks_game / tanks_rbac — 双设备演示

```text
RealmPolicyStore
  ├─ device:tank-a → TankAPolicyStore   + StoreIS(TankAUserStore)
  └─ device:tank-b → TankBPolicyStore   + StoreIS(TankBUserStore)
```

- C++ 内置演示数据（`Demo.*` 中的 `TankA*Store` / `TankB*Store`）— 无需外部 fixture 文件。
- 操作映射到权限（`action=forward;resource=device`、`action=fire;resource=device` 等）。
- 游戏路径 **只用 `checkPermission`**（移动/开火不自动弹窗）。
- 被拒绝 → 提权对话框：
  1. `authenticate(cred)`
  2. `checkSubjectPermission(perm, subject)`
  3. 若会话为空 → `activate`（登录）；否则 **一次性放行**，不替换会话。
- 显式登录（`L` / batch `login`）调用 `login()`；该领域先前 Login 会话在 `activate` 内清除。

---

## 9. 设计不变量

1. **登录会话 ≡ `IdentitySource::Login`。** Auto/匿名身份不会因领域切换登录而被清除。
2. 同一 `SecurityManager` 下 **每个领域最多一个 Login 会话**（activate 先清该领域）。
3. **跨领域的 Login 会话可并存**（例如同时登录 tank-a 与 tank-b）。
4. **一次性提权** 使用 `authenticate` + `checkSubjectPermission`，不调用 `activate`。
5. **登出抑制自动登录**，直到成功的交互/`login` 路径清除标志。
6. **`check*` 永不弹窗**；**`request*` 可以**。
7. 权限特异度相同时 **Deny 优先**。
8. **UserStore 角色仅为名称**；登录时角色变为身份才带上领域。
9. **RealmPolicyStore** 在同一 manager 下按设备/租户隔离策略数据。

---

## 10. 文件布局

```text
src/bas/security/
  Types.*                 IdentitySource/State, LoginStatus, AccessEffect
  Realm.*                 Realm + GLOBAL
  Identity.*              IdentityRef / Identity / IdentitySet
  Subject.hpp             Subject 辅助
  Permission.*            Permission + DefaultPermissionMatcher
  ACList.*                ACEntry, ACList
  Binding.*               AccessGrant, PolicyBinding, ACEMatch
  AccessDecision.hpp      AccessDecision, AccessRequestOptions
  AccessDecisionResolver.*  Decision + ACE 裁决策略
  AccessDenied.hpp
  PolicyStore.*           Default / File / Decorated / RealmPolicyStore
  UserStore.*             Default / File / RegistryUserStore
  PasswordDigest.*
  CredentialManager.*
  IdentityService.*       Anonymous + Store
  IdentityRegistry.*
  LoginUi.*
  SecurityManager.*
  Demo.*                  Demo + TankA/B 种子存储
  CommandSupport.*        CLI 辅助
  *Command.cpp            CLI 实现
```

---

## 11. 最小组装示例

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
sm->start();  // 适用时匿名自动登录
```

---

## 12. 相关入口

| 产物 | 用途 |
|------|------|
| `app/acdemo` | 交互式安全 REPL |
| `app/tanks_game` | Ncurses 双设备访问控制演示 |
| `tests/bas/security/security_test` | 匹配器、会话切换、一次性认证、存储 |
