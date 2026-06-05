#include <bas/security/AccessDecisionResolver.hpp>
#include <bas/security/AccessDenied.hpp>
#include <bas/security/CommandSupport.hpp>
#include <bas/security/CredentialManager.hpp>
#include <bas/security/IdentityRegistry.hpp>
#include <bas/security/IdentityService.hpp>
#include <bas/security/LoginPolicy.hpp>
#include <bas/security/LoginUi.hpp>
#include <bas/security/Permission.hpp>
#include <bas/security/PolicyStore.hpp>
#include <bas/security/Realm.hpp>
#include <bas/security/SecurityManager.hpp>
#include <bas/security/UserStore.hpp>

#include <bas/cli/opt_parser.h>
#include <bas/log/uselog.h>
#include <bas/util/repl.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <filesystem>

namespace sec = bas::security;

constexpr const char* kVersion = "0.1.0";
constexpr std::size_t kMaxHistoryLines = 5000;

std::filesystem::path defaultHistoryPath() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "acdemo" / "history";
    }
    return std::filesystem::path("acdemo-history");
}

std::vector<std::string> loadHistory(const std::filesystem::path& path) {
    std::vector<std::string> history;
    std::ifstream in(path);
    if (!in) {
        return history;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            history.push_back(std::move(line));
        }
    }
    if (history.size() > kMaxHistoryLines) {
        history.erase(history.begin(),
                      history.end() - static_cast<std::ptrdiff_t>(kMaxHistoryLines));
    }
    return history;
}

void saveHistory(const std::filesystem::path& path, const std::vector<std::string>& history) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return;
    }

    const std::size_t start =
        history.size() > kMaxHistoryLines ? history.size() - kMaxHistoryLines : 0;
    for (std::size_t i = start; i < history.size(); ++i) {
        out << history[i] << '\n';
    }
}

std::filesystem::path defaultCredentialPath() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "acdemo" / "credentials.json";
    }
    return std::filesystem::path("credentials.json");
}

std::filesystem::path defaultUserStorePath() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "acdemo" / "users.json";
    }
    return std::filesystem::path("users.json");
}

std::filesystem::path defaultAclPath() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / "acdemo" / "acl.json";
    }
    return std::filesystem::path("acl.json");
}

void printUsage(std::ostream& out) {
    out << "acdemo — bas.security permissions, credentials, and console login demo\n\n"
           "usage: acdemo [options] [command [args...]]\n\n"
           "  If no command is given, starts an interactive shell.\n\n"
           "commands:\n"
           "  check [@realm] PERMISSION       check permission (optional realm scope)\n"
           "  request [@realm] PERMISSION [USER]  request permission (login if needed)\n"
           "  login [@realm] [USER]             interactive login → active identities\n"
           "  whoami                            list active identities\n"
           "  creds [-v]                        list cached credentials (no secrets)\n"
           "  sm                                enter security manager shell\n"
           "  us                                enter user store shell\n"
           "  cm                                enter credential manager shell\n"
           "  ps                                enter policy store shell\n"
           "  reg                               enter identity registry shell\n"
           "  id                                enter identity service shell (current realm)\n"
           "  realm show|NAME|@realm            show or switch current realm\n"
           "  realms                            list registered realms\n"
           "  logout                            clear all active identities\n"
           "  logout-realm [NAME]               clear identities in a realm\n"
           "  reload-creds                      reload credentials and restore cached logins\n"
           "  help                              show this help\n\n"
           "  Interactive shells: sm, store, acl, cm, reg, id (exit or Ctrl-D to leave)\n"
           "  One-shot: store SUBCMD … | cm SUBCMD … | creds [-v]\n\n"
           "  @realm syntax:\n"
           "    @factory-a          realm name\n"
           "    @device:tablet-1    realm type + name (types: global, device, app)\n"
           "    @device             realm type only\n\n"
           "options:\n"
           "  (global options must appear before the subcommand, e.g. acdemo -d user.db store …)\n"
           "  -f, --credential-db PATH          credential cache file\n"
           "  -d, --user-db FILE                user store JSON file (default: built-in demo)\n"
           "  -p, --policy-db FILE              policy store JSON file (default: built-in demo)\n"
           "  -u, --subject USER                default subject hint\n"
           "  -r, --realm NAME                  default realm name\n"
           "      --realm-type TYPE             default realm type (global|device|app)\n"
           "      --realm-uuid UUID             default realm uuid\n"
           "      --version                     print version and exit\n"
           "  -h, --help                        show help and exit\n\n"
           "demo permissions (ACL is built-in):\n"
           "  fab.order.view / fab.order.modify / fab.order.delete / file.save\n";
}

std::size_t restoreCachedLogins(sec::SecurityManager& ac, const sec::Realm& realmFilter) {
    sec::AccessRequestOptions options;
    options.realmHint = realmFilter;
    return ac.activateCachedCredentials(options);
}

std::shared_ptr<sec::PolicyStore>
makePolicyStore(const std::optional<std::filesystem::path>& aclPath) {
    if (!aclPath.has_value()) {
        return std::make_shared<sec::DemoPolicyStore>();
    }
    auto store = std::make_shared<sec::FilePolicyStore>(
        *aclPath, std::make_shared<sec::DefaultPolicyStore>());
    if (store->grants().empty()) {
        sec::populateDemoPolicy(*store);
        store->persistToDisk();
    }
    return store;
}

std::shared_ptr<sec::IdentityRegistry>
makeDemoRegistry(const std::shared_ptr<sec::UserStore>& userStore,
                 const sec::Realm& defaultRealm = {}) {
    auto registry = std::make_shared<sec::IdentityRegistry>();
    auto anonymous = std::make_shared<sec::AnonymousIdentityService>();
    auto storeService = std::make_shared<sec::StoreIdentityService>(userStore);
    registry->add(anonymous);
    registry->add(storeService, sec::Realm::GLOBAL);
    if (!defaultRealm.empty() && !defaultRealm.scopedEqual(sec::Realm::GLOBAL)) {
        registry->add(storeService, defaultRealm);
    }
    return registry;
}

struct DemoContext {
    std::shared_ptr<sec::FileCredentialManager> credentials;
    std::shared_ptr<sec::UserStore> userStore;
    std::shared_ptr<sec::PolicyStore> policyStore;
    std::shared_ptr<sec::IdentityRegistry> registry;
    std::shared_ptr<sec::SecurityManager> sm;
};

std::shared_ptr<sec::IdentityService> identityServiceForCurrentRealm(const DemoContext& ctx) {
    sec::Realm realm = ctx.sm->commandDefaultRealm();
    if (realm.empty()) {
        const auto realms = ctx.sm->realms();
        if (!realms.empty()) {
            realm = realms.front();
        }
    }
    const auto services = ctx.registry->findByRealm(realm);
    for (const auto& service : services) {
        if (service && service->identityType() == "user" && !service->canAutoLogin()) {
            return service;
        }
    }
    for (const auto& service : services) {
        if (service) {
            return service;
        }
    }
    return nullptr;
}

void printRealmInfo(const sec::Realm& realm, bool current) {
    if (realm.empty()) {
        std::cout << "  (none)\n";
        return;
    }
    std::cout << "  label: " << realm.displayLabel() << '\n';
    if (!realm.type.empty()) {
        std::cout << "  type: " << realm.type << '\n';
    }
    if (!realm.name.empty()) {
        std::cout << "  name: " << realm.name << '\n';
    }
    if (!realm.uuid.empty()) {
        std::cout << "  uuid: " << realm.uuid << '\n';
    }
    if (!realm.description.empty()) {
        std::cout << "  description: " << realm.description << '\n';
    }
    if (current) {
        std::cout << "  (current)\n";
    }
}

bool switchCurrentRealm(DemoContext& ctx, const std::string& token) {
    sec::Realm realm;
    if (!token.empty() && token.front() == '@') {
        const auto parsed = sec::parseAtRealmToken(token);
        if (!parsed) {
            std::cerr << "invalid realm token: " << token << '\n';
            return false;
        }
        realm = ctx.sm->resolveRealmHint(*parsed);
    } else if (const auto found = ctx.sm->findRealmByName(token)) {
        realm = *found;
    } else if (sec::isKnownRealmType(token)) {
        realm.type = token;
        realm = ctx.sm->resolveRealmHint(realm);
    } else {
        realm.name = token;
        realm = ctx.sm->resolveRealmHint(realm);
    }
    ctx.sm->setCommandDefaultRealm(realm);
    std::cout << "current realm: " << realm.displayLabel() << '\n';
    return true;
}

sec::Realm effectiveCurrentRealm(const DemoContext& ctx) {
    sec::Realm realm = ctx.sm->commandDefaultRealm();
    if (!realm.empty()) {
        return ctx.sm->resolveRealmHint(realm);
    }
    const auto& realms = ctx.sm->realms();
    if (!realms.empty()) {
        return realms.front();
    }
    return realm;
}

bool isMarkedCurrentRealm(const DemoContext& ctx, const sec::Realm& realm) {
    const sec::Realm configured = ctx.sm->commandDefaultRealm();
    if (!configured.empty()) {
        return realm.same(configured);
    }
    return realm.same(effectiveCurrentRealm(ctx));
}

bool runRealmCommand(DemoContext& ctx, std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "usage: realm show | realm NAME | realm @token\n";
        return false;
    }
    const std::string head = sec::shiftArg(args);
    if (head == "show" || head == "?") {
        std::cout << "current realm:\n";
        printRealmInfo(effectiveCurrentRealm(ctx), true);
        return true;
    }
    return switchCurrentRealm(ctx, head);
}

bool runRealmsCommand(const DemoContext& ctx) {
    const auto& realms = ctx.sm->realms();
    if (realms.empty()) {
        std::cout << "no registered realms\n";
        return true;
    }
    std::cout << "registered realms:\n";
    for (const auto& realm : realms) {
        std::cout << "  " << realm.displayLabel();
        if (isMarkedCurrentRealm(ctx, realm)) {
            std::cout << " (current)";
        }
        std::cout << '\n';
    }
    return true;
}

namespace {

const std::vector<std::string> kTopLevelCommands = {
    "help",    "?",        "us",           "ps",     "cm",     "sm",     "reg",          "id",
    "creds",   "registry", "realm",        "realms", "whoami", "logout", "logout-realm", "check",
    "request", "login",    "reload-creds", "quit",   "exit",   "q",
};

const std::vector<std::string> kShellEntryCommands = {
    "us", "ps", "cm", "reg", "id", "sm", "registry",
};

const std::vector<std::string> kRealmSubCommands = {
    "show",
};

std::vector<std::string> completeCommand(const DemoContext& ctx,
                                         const std::vector<std::string>& args, std::size_t index) {
    const std::string cword = sec::currentWord(args, index);
    if (index == 0) {
        return sec::filterByPrefix(kTopLevelCommands, cword);
    }
    if (args.empty()) {
        return {};
    }
    const std::string& cmd = args[0];
    if (cmd == "us") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.userStore->complete(sub, subIndex);
    }
    if (cmd == "ps") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.policyStore->complete(sub, subIndex);
    }
    if (cmd == "cm") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.credentials->complete(sub, subIndex);
    }
    if (cmd == "sm") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.sm->complete(sub, subIndex);
    }
    if (cmd == "realm") {
        if (index == 1) {
            std::vector<std::string> options = kRealmSubCommands;
            for (const auto& realm : ctx.sm->realms()) {
                if (!realm.name.empty()) {
                    options.push_back(realm.name);
                    options.push_back("@" + realm.name);
                }
                if (!realm.type.empty()) {
                    options.push_back(realm.type);
                    options.push_back("@" + realm.type);
                }
            }
            options.push_back("@global");
            options.push_back("@device:");
            options.push_back("@app:");
            return sec::filterByPrefix(options, cword);
        }
        return {};
    }
    if (cmd == "reg" || cmd == "registry") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.registry->complete(sub, subIndex);
    }
    if (cmd == "id") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        if (const auto service = identityServiceForCurrentRealm(ctx)) {
            return service->complete(sub, subIndex);
        }
        return {};
    }
    return ctx.sm->complete(args, index);
}

} // namespace

std::vector<std::string> tokenize(std::string_view line);

bool runSubShell(sec::ICommandSupport& target, const std::string& name, const std::string& prompt,
                 std::vector<std::string>& history, bool& replExiting) {
    std::cout << "entered " << name << " (exit or Ctrl-D to leave)\n";
    const bas::util::repl::CompleteFn completer = [&target](const std::vector<std::string>& args,
                                                            std::size_t index) {
        return target.complete(args, index);
    };

    while (true) {
        std::string line;
        const auto status = bas::util::repl::readLine(prompt, completer, history, line, true);
        if (status == bas::util::repl::ReadLineStatus::Eof) {
            replExiting = true;
            return false;
        }
        if (status == bas::util::repl::ReadLineStatus::Pop) {
            return true;
        }
        if (line.empty()) {
            continue;
        }
        auto args = tokenize(line);
        if (args.empty()) {
            continue;
        }
        if (args[0] == "quit" || args[0] == "exit" || args[0] == "q") {
            return true;
        }
        target.invoke(args);
    }
}

bool enterSubShell(DemoContext& ctx, const std::string& cmd, std::vector<std::string>& history,
                   bool& replExiting) {
    if (cmd == "us" || cmd == "user-store") {
        return runSubShell(*ctx.userStore, "us", "user store> ", history, replExiting);
    }
    if (cmd == "ps" || cmd == "policy-store") {
        return runSubShell(*ctx.policyStore, "ps", "policy store> ", history, replExiting);
    }
    if (cmd == "cm" || cmd == "credential-manager") {
        return runSubShell(*ctx.credentials, "cm", "credential manager> ", history, replExiting);
    }
    if (cmd == "sm" || cmd == "security-manager") {
        return runSubShell(*ctx.sm, "sm", "security manager> ", history, replExiting);
    }
    if (cmd == "reg" || cmd == "identity-registry") {
        return runSubShell(*ctx.registry, "reg", "identity registry> ", history, replExiting);
    }
    if (cmd == "id" || cmd == "identity-service") {
        const auto service = identityServiceForCurrentRealm(ctx);
        if (!service) {
            std::cerr << "no identity service for current realm\n";
            return true;
        }
        return runSubShell(*service, "id", "identity service> ", history, replExiting);
    }
    return true;
}

bool isShellEntryCommand(const std::string& cmd) {
    return std::find(kShellEntryCommands.begin(), kShellEntryCommands.end(), cmd) !=
           kShellEntryCommands.end();
}

bool runCommand(DemoContext& ctx, std::vector<std::string> args) {
    if (args.empty()) {
        return true;
    }

    const std::string cmd = args[0];
    args.erase(args.begin());

    if (cmd == "help" || cmd == "?") {
        printUsage(std::cout);
        return true;
    }
    if (cmd == "us" || cmd == "user-store") {
        return ctx.userStore->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "ps" || cmd == "policy-store") {
        return ctx.policyStore->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "cm" || cmd == "credential-manager") {
        return ctx.credentials->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "sm" || cmd == "security-manager") {
        return ctx.sm->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "creds") {
        if (args.empty()) {
            args.push_back("list");
        }
        return ctx.credentials->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "realm") {
        return runRealmCommand(ctx, args);
    }
    if (cmd == "realms") {
        return runRealmsCommand(ctx);
    }
    if (cmd == "reg" || cmd == "identity-registry") {
        return ctx.registry->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "id" || cmd == "identity-service") {
        const auto service = identityServiceForCurrentRealm(ctx);
        if (!service) {
            std::cerr << "no identity service for current realm\n";
            return false;
        }
        return service->invoke(args) == sec::commandSuccess();
    }

    std::vector<std::string> acArgs;
    acArgs.push_back(cmd);
    acArgs.insert(acArgs.end(), args.begin(), args.end());
    return ctx.sm->invoke(acArgs) == sec::commandSuccess();
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

int runRepl(DemoContext& ctx, const std::filesystem::path& historyPath) {
    std::cout << "acdemo interactive mode. Type help for commands.\n";
    std::cout << "  Tab: complete   Up/Down: history   Emacs keys: Ctrl-A/E/F/B, Alt-F/B\n";
    std::cout << "  Ctrl-arrows: word   Ctrl-K/U/W Alt-BS: kill word   Ctrl-L: redraw\n";
    std::cout << "  Shells: con, store, acl, man, reg, id (exit or Ctrl-D to leave)\n";
    std::vector<std::string> whoami{"whoami"};
    ctx.sm->invoke(whoami);
    std::vector<std::string> credList;
    ctx.credentials->invoke(credList);
    std::cout << "credential file: " << ctx.credentials->credentialPath() << '\n';
    std::cout << "user store: " << ctx.userStore->storeLabel() << "\n\n";
    // std::cout << "policy store: " << ctx.policyStore->storeLabel() << "\n\n";

    std::vector<std::string> history = loadHistory(historyPath);
    const bas::util::repl::CompleteFn completer = [&ctx](const std::vector<std::string>& args,
                                                         std::size_t index) {
        return completeCommand(ctx, args, index);
    };

    bool replExiting = false;
    constexpr const char* kPrompt = "acdemo> ";
    std::string line;
    while (!replExiting) {
        const auto status = bas::util::repl::readLine(kPrompt, completer, history, line, false);
        if (status == bas::util::repl::ReadLineStatus::Eof) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        auto args = tokenize(line);
        if (args.empty()) {
            continue;
        }
        if (args[0] == "quit" || args[0] == "exit" || args[0] == "q") {
            break;
        }
        if (args.size() == 1 && isShellEntryCommand(args[0])) {
            enterSubShell(ctx, args[0], history, replExiting);
            continue;
        }
        runCommand(ctx, args);
    }
    saveHistory(historyPath, history);
    return 0;
}

DemoContext makeContext(const std::filesystem::path& credentialCachePath,
                        const std::optional<std::filesystem::path>& userStorePath,
                        const std::optional<std::filesystem::path>& policyStorePath,
                        std::string defaultSubject, sec::Realm defaultRealm) {
    DemoContext ctx;
    ctx.credentials = std::make_shared<sec::FileCredentialManager>(
        credentialCachePath, std::make_shared<sec::DefaultCredentialManager>());

    if (userStorePath.has_value()) {
        ctx.userStore = std::make_shared<sec::FileUserStore>(
            *userStorePath, std::make_shared<sec::DefaultUserStore>());
    } else {
        ctx.userStore = std::make_shared<sec::DemoUserStore>();
    }

    auto policyStore = makePolicyStore(policyStorePath);
    auto registry = makeDemoRegistry(ctx.userStore, defaultRealm);
    auto loginPolicy = std::make_shared<sec::LoginPolicy>();
    auto matcher = std::make_shared<sec::DefaultPermissionMatcher>();
    auto resolver = std::make_shared<sec::DefaultACResolvePolicy>();

    ctx.policyStore = policyStore;
    ctx.registry = registry;
    ctx.sm = std::make_shared<sec::SecurityManager>(policyStore, ctx.credentials, registry,
                                                    loginPolicy, matcher, resolver);
    auto consoleLogin = std::make_shared<sec::ConsoleLogin>(*ctx.sm);
    ctx.sm->setLoginUi(consoleLogin);
    ctx.sm->setCommandDefaults(std::move(defaultRealm), std::move(defaultSubject));
    return ctx;
}

int main(int argc, char** argv) {
    std::filesystem::path credentialCachePath = defaultCredentialPath();
    std::optional<std::filesystem::path> userStorePath;
    std::optional<std::filesystem::path> policyStorePath;
    std::string defaultSubject;
    sec::Realm defaultRealm;

    static const option longOpts[] = {
        {"credential-db", required_argument, nullptr, 'f'},
        {"user-db", required_argument, nullptr, 'd'},
        {"policy-db", required_argument, nullptr, 'p'},
        {"subject", required_argument, nullptr, 'u'},
        {"realm", required_argument, nullptr, 'r'},
        {"realm-type", required_argument, nullptr, 3},
        {"realm-uuid", required_argument, nullptr, 2},
        {"version", no_argument, nullptr, 1},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    opt_parser_t parser;
    opt_parser_init(&parser);

    int opt = 0;
    while ((opt = opt_parse_long(&parser, argc, argv, "f:u:r:hd:a:", longOpts, nullptr)) != -1) {
        switch (opt) {
        case 'f':
            credentialCachePath = parser.optarg;
            break;
        case 'd':
            userStorePath = parser.optarg;
            break;
        case 'p':
            policyStorePath = parser.optarg;
            break;
        case 'u':
            defaultSubject = parser.optarg;
            break;
        case 'r':
            defaultRealm.name = parser.optarg;
            break;
        case 3:
            defaultRealm.type = parser.optarg;
            break;
        case 2:
            defaultRealm.uuid = parser.optarg;
            break;
        case 1:
            std::cout << "acdemo " << kVersion << '\n';
            return 0;
        case 'h':
            printUsage(std::cout);
            return 0;
        default:
            printUsage(std::cerr);
            return 2;
        }
    }

    std::vector<std::string> cmdArgs;
    for (int i = parser.optind; i < argc; ++i) {
        cmdArgs.emplace_back(argv[i]);
    }

    try {
        DemoContext ctx =
            makeContext(credentialCachePath, userStorePath, policyStorePath, defaultSubject, defaultRealm);

        logdebug_fmt("credential cache: %s", ctx.credentials->credentialPath().c_str());
        logdebug_fmt("user store %s", ctx.userStore->storeLabel().c_str());

        const auto storeFile = ctx.userStore->storePath();
        if (!storeFile.empty()) {
            logdebug_fmt("store file: %s", storeFile.c_str());
        }
        if (!defaultRealm.empty()) {
            logdebug_fmt("default realm: %s", defaultRealm.str().c_str());
        }
        if (ctx.credentials->credentialPersistedCount() > 0) {
            logdebug_fmt("loaded %d cached credential(s) from disk",
                         ctx.credentials->credentialPersistedCount());
        }

        ctx.sm->start();
        restoreCachedLogins(*ctx.sm, {});

        if (cmdArgs.empty()) {
            std::vector<std::string> whoami{"whoami"};
            ctx.sm->invoke(whoami);
            std::cout << '\n';
            return runRepl(ctx, defaultHistoryPath());
        }
        return runCommand(ctx, cmdArgs) ? 0 : 1;
    } catch (const sec::AccessDenied& e) {
        logerror_fmt("access denied: %s", e.what());
        return 1;
    } catch (const std::exception& e) {
        logerror_fmt("error: %s", e.what());
        return 1;
    }
}
