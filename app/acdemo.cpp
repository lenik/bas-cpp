#include <bas/security/ac_list.hpp>
#include <bas/security/access_controller.hpp>
#include <bas/security/ac_rule.hpp>
#include <bas/security/access_denied.hpp>
#include <bas/security/command_support.hpp>
#include <bas/security/credential.hpp>
#include <bas/security/identity_registry.hpp>
#include <bas/security/identity_service.hpp>
#include <bas/security/login_ui.hpp>
#include <bas/security/login_policy.hpp>
#include <bas/security/permission.hpp>
#include <bas/security/realm.hpp>
#include <bas/security/user_store.hpp>

#include <bas/util/repl.hpp>

#include <getopt.h>

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
           "  man                               enter credential manager shell\n"
           "  con                               enter access controller shell\n"
           "  store                             enter user store shell\n"
           "  acl                               enter ACL shell\n"
           "  reg                               enter identity registry shell\n"
           "  id                                enter identity service shell (current realm)\n"
           "  realm show|NAME|@realm            show or switch current realm\n"
           "  realms                            list registered realms\n"
           "  logout                            clear all active identities\n"
           "  logout-realm [NAME]               clear identities in a realm\n"
           "  reload-creds                      reload credentials and restore cached logins\n"
           "  help                              show this help\n\n"
           "  Interactive shells: con, store, acl, man, reg, id (exit or Ctrl-D to leave)\n"
           "  One-shot: store SUBCMD … | man SUBCMD … | creds [-v]\n\n"
           "  @realm syntax:\n"
           "    @factory-a          realm name\n"
           "    @device:tablet-1    realm type + name (types: global, device, app)\n"
           "    @device             realm type only\n\n"
           "options:\n"
           "  -f, --cred-file PATH              credential cache file\n"
           "  -d, --store FILE                  user store JSON file (default: built-in demo)\n"
           "  -a, --acl FILE                    ACL JSON file (default: built-in demo rules)\n"
           "  -u, --subject USER                default subject hint\n"
           "  -r, --realm NAME                  default realm name\n"
           "      --realm-type TYPE             default realm type (global|device|app)\n"
           "      --realm-uuid UUID             default realm uuid\n"
           "      --version                     print version and exit\n"
           "  -h, --help                        show help and exit\n\n"
           "demo permissions (ACL is built-in):\n"
           "  fab.order.view / fab.order.modify / fab.order.delete / file.save\n";
}

std::size_t restoreCachedLogins(sec::AccessController& ac, const sec::Realm& realmFilter) {
    sec::ACRequestOptions options;
    options.realmHint = realmFilter;
    return ac.activateCachedCredentials(options);
}

std::shared_ptr<sec::ACList> makeAcl(const std::optional<std::filesystem::path>& aclPath) {
    if (!aclPath.has_value()) {
        return std::make_shared<sec::DemoACList>();
    }
    auto acl = std::make_shared<sec::FileACList>(*aclPath,
                                                 std::make_unique<sec::DefaultACList>());
    if (acl->entries().empty()) {
        sec::populateDemoAcl(*acl);
        acl->flush();
    }
    return acl;
}

std::shared_ptr<sec::IdentityRegistry> makeDemoRegistry(
    const std::shared_ptr<sec::UserStore>& userStore, const sec::Realm& defaultRealm = {}) {
    auto registry = std::make_shared<sec::IdentityRegistry>();
    auto anonymous = std::make_shared<sec::AnonymousIdentityService>();
    auto storeService = std::make_shared<sec::StoreIdentityService>(userStore);
    registry->add(anonymous);
    registry->add(storeService);
    registry->registerRealm(sec::Realm::GLOBAL, storeService);
    for (const auto& realm : userStore->getRealms()) {
        registry->registerRealm(realm, storeService);
    }
    if (!defaultRealm.empty()) {
        registry->registerRealm(defaultRealm, storeService);
    }
    return registry;
}

struct DemoContext {
    std::shared_ptr<sec::FileCredentialManager> credentials;
    std::shared_ptr<sec::UserStore> userStore;
    std::shared_ptr<sec::ACList> acl;
    std::shared_ptr<sec::IdentityRegistry> registry;
    std::shared_ptr<sec::AccessController> ac;
};

std::shared_ptr<sec::IdentityService> identityServiceForCurrentRealm(const DemoContext& ctx) {
    sec::Realm realm = ctx.ac->commandDefaultRealm();
    if (realm.empty()) {
        const auto realms = ctx.ac->realms();
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
    std::cout << "  label: " << sec::realmDisplayLabel(realm) << '\n';
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
        realm = ctx.ac->resolveRealmHint(*parsed);
    } else if (const auto found = ctx.ac->findRealmByName(token)) {
        realm = *found;
    } else if (sec::isKnownRealmType(token)) {
        realm.type = token;
        realm = ctx.ac->resolveRealmHint(realm);
    } else {
        realm.name = token;
        realm = ctx.ac->resolveRealmHint(realm);
    }
    ctx.ac->setCommandDefaultRealm(realm);
    std::cout << "current realm: " << sec::realmDisplayLabel(realm) << '\n';
    return true;
}

sec::Realm effectiveCurrentRealm(const DemoContext& ctx) {
    sec::Realm realm = ctx.ac->commandDefaultRealm();
    if (!realm.empty()) {
        return ctx.ac->resolveRealmHint(realm);
    }
    const auto& realms = ctx.ac->realms();
    if (!realms.empty()) {
        return realms.front();
    }
    return realm;
}

bool isMarkedCurrentRealm(const DemoContext& ctx, const sec::Realm& realm) {
    const sec::Realm configured = ctx.ac->commandDefaultRealm();
    if (!configured.empty()) {
        return sec::realmSame(realm, configured);
    }
    return sec::realmSame(realm, effectiveCurrentRealm(ctx));
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
    const auto& realms = ctx.ac->realms();
    if (realms.empty()) {
        std::cout << "no registered realms\n";
        return true;
    }
    std::cout << "registered realms:\n";
    for (const auto& realm : realms) {
        std::cout << "  " << sec::realmDisplayLabel(realm);
        if (isMarkedCurrentRealm(ctx, realm)) {
            std::cout << " (current)";
        }
        std::cout << '\n';
    }
    return true;
}

namespace {

const std::vector<std::string> kTopLevelCommands = {
    "help",   "?",        "store",  "acl",          "man",      "con",        "reg",        "id",
    "creds",  "registry", "realm",  "realms",       "whoami",   "logout",     "logout-realm",
    "check",  "request",  "login",  "reload-creds", "quit",     "exit",       "q",
};

const std::vector<std::string> kShellEntryCommands = {
    "store", "acl", "man", "reg", "id", "con", "registry",
};

const std::vector<std::string> kRealmSubCommands = {
    "show",
};

std::vector<std::string> completeCommand(const DemoContext& ctx,
                                         const std::vector<std::string>& args,
                                         std::size_t index) {
    const std::string cword = sec::currentWord(args, index);
    if (index == 0) {
        return sec::filterByPrefix(kTopLevelCommands, cword);
    }
    if (args.empty()) {
        return {};
    }
    const std::string& cmd = args[0];
    if (cmd == "store") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.userStore->complete(sub, subIndex);
    }
    if (cmd == "acl") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.acl->complete(sub, subIndex);
    }
    if (cmd == "man") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.credentials->complete(sub, subIndex);
    }
    if (cmd == "con") {
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return ctx.ac->complete(sub, subIndex);
    }
    if (cmd == "realm") {
        if (index == 1) {
            std::vector<std::string> options = kRealmSubCommands;
            for (const auto& realm : ctx.ac->realms()) {
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
    return ctx.ac->complete(args, index);
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
        const auto status =
            bas::util::repl::readLine(prompt, completer, history, line, true);
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
    if (cmd == "store") {
        return runSubShell(*ctx.userStore, "store", "store> ", history, replExiting);
    }
    if (cmd == "acl") {
        return runSubShell(*ctx.acl, "acl", "acl> ", history, replExiting);
    }
    if (cmd == "man") {
        return runSubShell(*ctx.credentials, "man", "man> ", history, replExiting);
    }
    if (cmd == "con") {
        return runSubShell(*ctx.ac, "con", "con> ", history, replExiting);
    }
    if (cmd == "reg" || cmd == "registry") {
        return runSubShell(*ctx.registry, "reg", "reg> ", history, replExiting);
    }
    if (cmd == "id") {
        const auto service = identityServiceForCurrentRealm(ctx);
        if (!service) {
            std::cerr << "no identity service for current realm\n";
            return true;
        }
        return runSubShell(*service, "id", "id> ", history, replExiting);
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
    if (cmd == "store") {
        return ctx.userStore->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "acl") {
        return ctx.acl->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "man") {
        return ctx.credentials->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "con") {
        return ctx.ac->invoke(args) == sec::commandSuccess();
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
    if (cmd == "reg" || cmd == "registry") {
        return ctx.registry->invoke(args) == sec::commandSuccess();
    }
    if (cmd == "id") {
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
    return ctx.ac->invoke(acArgs) == sec::commandSuccess();
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
    ctx.ac->invoke(whoami);
    std::vector<std::string> credList;
    ctx.credentials->invoke(credList);
    std::cout << "credential file: " << ctx.credentials->credentialPath() << '\n';
    std::cout << "user store: " << ctx.userStore->storeLabel() << "\n\n";

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

DemoContext makeContext(const std::filesystem::path& credPath,
                        const std::optional<std::filesystem::path>& storePath,
                        const std::optional<std::filesystem::path>& aclPath,
                        std::string defaultSubject, sec::Realm defaultRealm) {
    DemoContext ctx;
    ctx.credentials = std::make_shared<sec::FileCredentialManager>(
        credPath, std::make_unique<sec::DefaultCredentialManager>());

    if (storePath.has_value()) {
        ctx.userStore = std::make_shared<sec::FileUserStore>(
            *storePath, std::make_unique<sec::DefaultUserStore>());
    } else {
        ctx.userStore = std::make_shared<sec::DemoUserStore>();
    }

    auto acl = makeAcl(aclPath);
    auto registry = makeDemoRegistry(ctx.userStore, defaultRealm);
    auto loginPolicy = std::make_shared<sec::LoginPolicy>();
    auto matcher = std::make_shared<sec::DefaultPermissionMatcher>();
    auto resolver = std::make_shared<sec::DefaultACResolvePolicy>();

    ctx.acl = acl;
    ctx.registry = registry;
    ctx.ac = std::make_shared<sec::AccessController>(acl, ctx.credentials, registry, loginPolicy,
                                                     matcher, resolver);
    auto consoleLogin = std::make_shared<sec::ConsoleLogin>(*ctx.ac);
    ctx.ac->setLoginUi(consoleLogin);
    ctx.ac->setCommandDefaults(std::move(defaultRealm), std::move(defaultSubject));
    return ctx;
}

int main(int argc, char** argv) {
    std::filesystem::path credPath = defaultCredentialPath();
    std::optional<std::filesystem::path> storePath;
    std::optional<std::filesystem::path> aclPath;
    std::string defaultSubject;
    sec::Realm defaultRealm;

    static const option longOpts[] = {
        {"cred-file", required_argument, nullptr, 'f'},
        {"store", required_argument, nullptr, 'd'},
        {"acl", required_argument, nullptr, 'a'},
        {"subject", required_argument, nullptr, 'u'},
        {"realm", required_argument, nullptr, 'r'},
        {"realm-type", required_argument, nullptr, 3},
        {"realm-uuid", required_argument, nullptr, 2},
        {"version", no_argument, nullptr, 1},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "f:u:r:hd:a:", longOpts, nullptr)) != -1) {
        switch (opt) {
        case 'f':
            credPath = optarg;
            break;
        case 'd':
            storePath = optarg;
            break;
        case 'a':
            aclPath = optarg;
            break;
        case 'u':
            defaultSubject = optarg;
            break;
        case 'r':
            defaultRealm.name = optarg;
            break;
        case 3:
            defaultRealm.type = optarg;
            break;
        case 2:
            defaultRealm.uuid = optarg;
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
    for (int i = optind; i < argc; ++i) {
        cmdArgs.emplace_back(argv[i]);
    }

    try {
        DemoContext ctx = makeContext(credPath, storePath, aclPath, defaultSubject, defaultRealm);

        std::cout << "credential cache: " << ctx.credentials->credentialPath() << '\n';
        std::cout << "user store: " << ctx.userStore->storeLabel();
        const auto storeFile = ctx.userStore->storePath();
        if (!storeFile.empty()) {
            std::cout << " (" << storeFile << ')';
        }
        std::cout << '\n';
        if (!defaultRealm.empty()) {
            std::cout << "default";
            sec::writeRealmSuffix(std::cout, defaultRealm);
            std::cout << '\n';
        }
        if (ctx.credentials->credentialPersistedCount() > 0) {
            std::cout << "loaded " << ctx.credentials->credentialPersistedCount()
                      << " cached credential(s) from disk\n";
        }

        ctx.ac->start();
        restoreCachedLogins(*ctx.ac, {});

        if (cmdArgs.empty()) {
            std::vector<std::string> whoami{"whoami"};
            ctx.ac->invoke(whoami);
            std::cout << '\n';
            return runRepl(ctx, defaultHistoryPath());
        }
        return runCommand(ctx, cmdArgs) ? 0 : 1;
    } catch (const sec::AccessDenied& e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "acdemo: " << e.what() << '\n';
        return 1;
    }
}
