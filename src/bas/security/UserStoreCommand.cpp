#include "UserStore.hpp"

#include "CommandSupport.hpp"
#include "PasswordDigest.hpp"

#include "../reg/Registry.hpp"
#include "../reg/Sub.hpp"

#include <boost/json.hpp>

#include <chrono>
#include <iostream>
#include <sstream>

namespace bas::security {

namespace {

struct UserWriteOptions {
    std::string userName;
    std::optional<std::string> password;
    bool wantPassword = false;
    std::string display;
    std::string email;
    std::string avatar;
    std::string hashAlgo = "plain";
    std::vector<std::string> roles;
    bool rolesSpecified = false;
};

void printUserStoreHelp(std::ostream& out) {
    out << "user store commands:\n"
           "  user list                       list usernames\n"
           "  user show USER                  show profile, roles, keys\n"
           "  user add [options] USER [PASSWORD]\n"
           "  user update [options] USER [PASSWORD]\n"
           "      -a/--avatar NAME   -d/--display NAME   -e/--email EMAIL\n"
           "      -p/--password PASS  [PASSWORD] positional\n"
           "      -H/--hash ALGO      plain (default), sha256, sha1, md5\n"
           "      -h/--help           show command help\n"
           "      -r/--roles ROLES    comma-separated role names\n"
           "      password from stdin when -p and [PASSWORD] are both omitted\n"
           "  user check USER [PASSWORD]        validate password (exit 1 on failure)\n"
           "  user del USER                   remove user\n"
           "  user enable USER | user disable USER\n"
           "  user roles USER | user roles set USER R…\n"
           "  user role add USER ROLE | user role del USER ROLE\n"
           "  user keys USER | user key add/del …\n"
           "  path | reload | save | help\n"
           "  commands may be abbreviated by unique prefix\n"
           "  -h/--help on any subcommand shows its usage\n";
}

void printUserListHelp(std::ostream& out) {
    out << "usage: user list [-h]\n"
           "  List all usernames in the store.\n";
}

void printUserShowHelp(std::ostream& out) {
    out << "usage: user show [-h] USER\n"
           "  Show profile, roles, and auth keys for USER.\n";
}

void printUserAddHelp(std::ostream& out) {
    out << "usage: user add [-h] [options] USER [PASSWORD]\n"
           "  Create a new user record.\n"
           "options:\n"
           "  -a, --avatar NAME     profile.attributes.avatar\n"
           "  -d, --display NAME    display name\n"
           "  -e, --email EMAIL     email address\n"
           "  -p, --password PASS   password (or positional PASSWORD)\n"
           "  -H, --hash ALGO       plain (default), sha256, sha1, md5\n"
           "  -r, --roles ROLES     comma-separated role names (default: operator)\n"
           "  -h, --help            show this help\n"
           "  If neither -p nor PASSWORD is given, password is read from stdin.\n";
}

void printUserUpdateHelp(std::ostream& out) {
    out << "usage: user update [-h] [options] USER [PASSWORD]\n"
           "  Update an existing user; only specified fields are changed.\n"
           "options: same as user add\n";
}

void printUserCheckHelp(std::ostream& out) {
    out << "usage: user check [-h] USER [PASSWORD]\n"
           "  Validate PASSWORD against USER's stored keys (exit 1 on failure).\n"
           "  PASSWORD from stdin when omitted.\n";
}

void printUserDelHelp(std::ostream& out) {
    out << "usage: user del|rm [-h] USER\n"
           "  Remove USER from the store.\n";
}

void printUserEnableHelp(std::ostream& out) {
    out << "usage: user enable|disable [-h] USER\n"
           "  Enable or disable USER login.\n";
}

void printUserRolesHelp(std::ostream& out) {
    out << "usage: user roles [-h] USER\n"
           "       user roles set [-h] USER ROLE…\n"
           "  List roles, or replace the role list for USER.\n";
}

void printUserRoleHelp(std::ostream& out) {
    out << "usage: user role add [-h] USER ROLE\n"
           "       user role del [-h] USER ROLE\n"
           "  Add or remove a single role name on USER.\n";
}

void printUserKeysHelp(std::ostream& out) {
    out << "usage: user keys [-h] USER\n"
           "  List authentication keys for USER.\n";
}

void printUserKeyHelp(std::ostream& out) {
    out << "usage: user key add [-h] USER ID TYPE [options]\n"
           "       user key del [-h] USER ID\n"
           "options for add:\n"
           "  -p, --password PASS   key secret\n"
           "  -H, --hash ALGO       for password-hash (default plain → password-plain)\n"
           "  --name LABEL          key display name\n"
           "  -h, --help            show this help\n";
}

void printUserRecord(const UserRecord& record) {
    const auto& p = record.profile;
    std::cout << "  name: " << p.name << '\n';
    if (!p.displayName.empty()) {
        std::cout << "  display: " << p.displayName << '\n';
    }
    if (!p.email.empty()) {
        std::cout << "  email: " << p.email << '\n';
    }
    if (auto* avatar = p.attributes.if_contains("avatar")) {
        if (avatar->is_string()) {
            std::cout << "  avatar: " << avatar->as_string().c_str() << '\n';
        }
    }
    std::cout << "  enabled: " << (p.enabled ? "yes" : "no") << '\n';
    std::cout << "  roles:";
    if (record.roles.empty()) {
        std::cout << " (none)\n";
    } else {
        for (const auto& role : record.roles) {
            std::cout << ' ' << role;
        }
        std::cout << '\n';
    }
    std::cout << "  keys:\n";
    if (record.keys.empty()) {
        std::cout << "    (none)\n";
    } else {
        for (const auto& key : record.keys) {
            std::cout << "    " << key.id << " type=" << key.type << " name=" << key.name
                      << " enabled=" << (key.enabled ? "yes" : "no");
            if (key.type == "password-hash") {
                if (auto* algo = key.data.if_contains("algorithm")) {
                    if (algo->is_string()) {
                        std::cout << " algo=" << algo->as_string().c_str();
                    }
                }
                if (auto* hash = key.data.if_contains("hash")) {
                    if (hash->is_string()) {
                        std::cout << " hash=" << hash->as_string().c_str();
                    }
                }
            } else if (key.type == "password-plain") {
                if (auto* password = key.data.if_contains("password")) {
                    if (password->is_string()) {
                        std::cout << " password=" << password->as_string().c_str();
                    }
                }
            }
            std::cout << '\n';
        }
    }
}

const std::vector<std::string> kUserCommands = {
    "list",  "show",   "add",    "update", "check", "del",    "rm",     "enable", "disable",
    "roles", "role",   "keys",   "key",    "help",
};

const std::vector<std::string> kStoreCommands = {
    "user", "help", "path", "reload", "save", "list", "show", "add", "update", "check", "del",
};

const std::vector<std::string> kRoleSubCommands = {"add", "del"};
const std::vector<std::string> kKeySubCommands = {"add", "del"};

std::string trimCopy(std::string_view text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

std::vector<std::string> splitCommaList(std::string_view text) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == ',') {
            const std::string item = trimCopy(current);
            if (!item.empty()) {
                parts.push_back(item);
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    const std::string item = trimCopy(current);
    if (!item.empty()) {
        parts.push_back(item);
    }
    return parts;
}

bool readPasswordFromStdin(std::string& password) {
    if (!std::getline(std::cin, password)) {
        return false;
    }
    if (!password.empty() && password.back() == '\r') {
        password.pop_back();
    }
    return true;
}

bool resolvePasswordInput(UserWriteOptions& opts, std::string& error) {
    if (opts.password.has_value()) {
        return true;
    }
    if (!opts.wantPassword) {
        return true;
    }
    std::string fromStdin;
    if (!readPasswordFromStdin(fromStdin)) {
        error = "failed to read password from stdin";
        return false;
    }
    opts.password = std::move(fromStdin);
    return true;
}

bool parseUserWriteOptions(std::vector<std::string>& args, UserWriteOptions& opts, bool requireUser,
                           bool readStdinIfMissing, std::string& error) {
    for (std::size_t i = 0; i < args.size();) {
        const std::string& token = args[i];
        auto takeValue = [&](std::string& out) -> bool {
            if (i + 1 >= args.size()) {
                error = "missing value for " + token;
                return false;
            }
            out = args[i + 1];
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                       args.begin() + static_cast<std::ptrdiff_t>(i + 2));
            return true;
        };

        if (token == "-a" || token == "--avatar") {
            if (!takeValue(opts.avatar)) {
                return false;
            }
            continue;
        }
        if (token == "-d" || token == "--display") {
            if (!takeValue(opts.display)) {
                return false;
            }
            continue;
        }
        if (token == "-p" || token == "--password") {
            std::string value;
            if (!takeValue(value)) {
                return false;
            }
            opts.password = std::move(value);
            opts.wantPassword = true;
            continue;
        }
        if (token == "-e" || token == "--email") {
            if (!takeValue(opts.email)) {
                return false;
            }
            continue;
        }
        if (token == "-H" || token == "--hash") {
            if (!takeValue(opts.hashAlgo)) {
                return false;
            }
            continue;
        }
        if (token == "-h" || token == "--help") {
            error = "help requested";
            return false;
        }
        if (token == "-r" || token == "--roles") {
            std::string value;
            if (!takeValue(value)) {
                return false;
            }
            opts.roles = splitCommaList(value);
            opts.rolesSpecified = true;
            continue;
        }
        if (!token.empty() && token.front() == '-') {
            error = "unknown option: " + token;
            return false;
        }
        ++i;
    }

    if (!args.empty()) {
        if (opts.userName.empty()) {
            opts.userName = args.front();
            args.erase(args.begin());
        }
    }
    if (!args.empty()) {
        if (!opts.password.has_value()) {
            opts.password = args.front();
            opts.wantPassword = true;
            args.erase(args.begin());
        }
    }
    if (!args.empty()) {
        error = "unexpected argument: " + args.front();
        return false;
    }
    if (requireUser && opts.userName.empty()) {
        error = "USER name required";
        return false;
    }
    if (readStdinIfMissing && !opts.password.has_value()) {
        opts.wantPassword = true;
    }
    if (opts.wantPassword && !opts.password.has_value()) {
        if (!resolvePasswordInput(opts, error)) {
            return false;
        }
    }
    return true;
}

bool parseCheckOptions(std::vector<std::string>& args, UserWriteOptions& opts, std::string& error) {
    if (args.empty()) {
        error = "USER name required";
        return false;
    }
    opts.userName = args.front();
    args.erase(args.begin());
    if (!args.empty()) {
        opts.password = args.front();
        opts.wantPassword = true;
        args.erase(args.begin());
    } else {
        opts.wantPassword = true;
    }
    if (!args.empty()) {
        error = "unexpected argument: " + args.front();
        return false;
    }
    return resolvePasswordInput(opts, error);
}

void applyProfileFields(UserProfile& profile, const UserWriteOptions& opts) {
    if (!opts.display.empty()) {
        profile.displayName = opts.display;
    }
    if (!opts.email.empty()) {
        profile.email = opts.email;
    }
    if (!opts.avatar.empty()) {
        profile.attributes["avatar"] = boost::json::value(opts.avatar);
    }
}

void setMainPasswordKey(UserStore& store, const std::string& userName, const std::string& password,
                        const std::string& hashAlgo) {
    const auto keys = store.getKeys(userName);
    for (const auto& key : keys) {
        if (key.id == "pwd-main") {
            store.removeKey(userName, "pwd-main");
        }
    }
    store.addKey(userName, makePasswordKeyForAlgorithm("pwd-main", password, hashAlgo));
}

int runUserAdd(UserStore& store, std::vector<std::string> args) {
    if (takeHelpRequest(args)) {
        printUserAddHelp(std::cout);
        return commandSuccess();
    }
    UserWriteOptions opts;
    std::string error;
    if (!parseUserWriteOptions(args, opts, true, true, error)) {
        if (error == "help requested") {
            printUserAddHelp(std::cout);
            return commandSuccess();
        }
        std::cerr << error << '\n';
        return commandFailure();
    }
    if (store.hasUser(opts.userName)) {
        std::cerr << "user already exists: " << opts.userName << '\n';
        return commandFailure();
    }
    if (opts.password.has_value() && !isPasswordStorageAlgorithm(opts.hashAlgo)) {
        std::cerr << "unsupported hash algorithm: " << opts.hashAlgo
                  << " (use plain, sha256, sha1, or md5)\n";
        return commandFailure();
    }

    UserRecord record;
    record.profile.name = opts.userName;
    record.profile.displayName = opts.display.empty() ? opts.userName : opts.display;
    record.profile.email = opts.email;
    record.profile.enabled = true;
    applyProfileFields(record.profile, opts);
    if (opts.rolesSpecified) {
        record.roles = opts.roles;
    } else if (record.roles.empty()) {
        record.roles.push_back("operator");
    }
    if (opts.password.has_value()) {
        record.keys.push_back(
            makePasswordKeyForAlgorithm("pwd-main", *opts.password, opts.hashAlgo));
    }
    store.addUser(record);
    std::cout << "added user " << opts.userName << '\n';
    return commandSuccess();
}

int runUserUpdate(UserStore& store, std::vector<std::string> args) {
    if (takeHelpRequest(args)) {
        printUserUpdateHelp(std::cout);
        return commandSuccess();
    }
    UserWriteOptions opts;
    std::string error;
    if (!parseUserWriteOptions(args, opts, true, true, error)) {
        if (error == "help requested") {
            printUserUpdateHelp(std::cout);
            return commandSuccess();
        }
        std::cerr << error << '\n';
        return commandFailure();
    }
    if (!store.hasUser(opts.userName)) {
        std::cerr << "user not found: " << opts.userName << '\n';
        return commandFailure();
    }
    if (opts.password.has_value() && !isPasswordStorageAlgorithm(opts.hashAlgo)) {
        std::cerr << "unsupported hash algorithm: " << opts.hashAlgo
                  << " (use plain, sha256, sha1, or md5)\n";
        return commandFailure();
    }

    auto record = store.getUserRecord(opts.userName);
    if (!record.has_value()) {
        std::cerr << "user not found: " << opts.userName << '\n';
        return commandFailure();
    }

    applyProfileFields(record->profile, opts);
    if (!opts.display.empty()) {
        record->profile.displayName = opts.display;
    }
    if (opts.rolesSpecified) {
        record->roles = opts.roles;
    }
    store.updateUserProfile(opts.userName, record->profile);
    if (opts.rolesSpecified) {
        store.setRoles(opts.userName, record->roles);
    }
    if (opts.password.has_value()) {
        setMainPasswordKey(store, opts.userName, *opts.password, opts.hashAlgo);
    }
    std::cout << "updated user " << opts.userName << '\n';
    return commandSuccess();
}

int runUserCheck(const UserStore& store, std::vector<std::string> args) {
    if (takeHelpRequest(args)) {
        printUserCheckHelp(std::cout);
        return commandSuccess();
    }
    UserWriteOptions opts;
    std::string error;
    if (!parseCheckOptions(args, opts, error)) {
        std::cerr << error << '\n';
        return commandFailure();
    }
    if (!opts.password.has_value()) {
        std::cerr << "password required\n";
        return commandFailure();
    }
    if (!store.hasUser(opts.userName)) {
        std::cerr << "user not found: " << opts.userName << '\n';
        return commandFailure();
    }
    if (verifyUserPassword(store, opts.userName, *opts.password)) {
        std::cout << "password ok for " << opts.userName << '\n';
        return commandSuccess();
    }
    std::cerr << "invalid password for " << opts.userName << '\n';
    return commandFailure();
}

bool resolveSubcommand(const std::vector<std::string>& commands, const std::string& token,
                       std::string& resolved) {
    const ResolvedCommand match = resolveCommandByPrefix(commands, token);
    if (match.match == CommandMatch::Ambiguous) {
        std::cerr << "ambiguous command: " << token << '\n';
        return false;
    }
    if (match.match == CommandMatch::NotFound) {
        std::cerr << "unknown command: " << token << '\n';
        return false;
    }
    resolved = match.value;
    return true;
}

} // namespace

int UserStore::invokeUser(std::vector<std::string>& args) {
    if (args.empty()) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }
    if (argsAreOnlyHelpFlags(args)) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }

    std::string sub = shiftArg(args);
    if (!resolveSubcommand(kUserCommands, sub, sub)) {
        return commandFailure();
    }

    if (sub == "help" || sub == "?") {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }
    if (sub == "list") {
        if (takeHelpRequest(args)) {
            printUserListHelp(std::cout);
            return commandSuccess();
        }
        const auto users = listUsers();
        if (users.empty()) {
            std::cout << "no users in store\n";
            return commandSuccess();
        }
        for (const auto& name : users) {
            const auto profile = getUserProfile(name);
            std::cout << "  " << name;
            if (profile.has_value()) {
                if (!profile->displayName.empty() && profile->displayName != name) {
                    std::cout << " (" << profile->displayName << ')';
                }
                if (!profile->enabled) {
                    std::cout << " [disabled]";
                }
            }
            std::cout << '\n';
        }
        return commandSuccess();
    }
    if (sub == "show") {
        if (takeHelpRequest(args)) {
            printUserShowHelp(std::cout);
            return commandSuccess();
        }
        if (args.empty()) {
            printUserShowHelp(std::cerr);
            return commandFailure();
        }
        const auto record = getUserRecord(args[0]);
        if (!record.has_value()) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        printUserRecord(*record);
        return commandSuccess();
    }
    if (sub == "add") {
        return runUserAdd(*this, args);
    }
    if (sub == "update") {
        return runUserUpdate(*this, args);
    }
    if (sub == "check") {
        return runUserCheck(*this, args);
    }
    if (sub == "del" || sub == "rm") {
        if (takeHelpRequest(args)) {
            printUserDelHelp(std::cout);
            return commandSuccess();
        }
        if (args.empty()) {
            printUserDelHelp(std::cerr);
            return commandFailure();
        }
        if (!hasUser(args[0])) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        removeUser(args[0]);
        std::cout << "removed user " << args[0] << '\n';
        return commandSuccess();
    }
    if (sub == "enable" || sub == "disable") {
        if (takeHelpRequest(args)) {
            printUserEnableHelp(std::cout);
            return commandSuccess();
        }
        if (args.empty()) {
            printUserEnableHelp(std::cerr);
            return commandFailure();
        }
        if (!hasUser(args[0])) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        if (sub == "enable") {
            enableUser(args[0]);
        } else {
            disableUser(args[0]);
        }
        std::cout << sub << "d user " << args[0] << '\n';
        return commandSuccess();
    }
    if (sub == "roles") {
        if (takeHelpRequest(args)) {
            printUserRolesHelp(std::cout);
            return commandSuccess();
        }
        if (args.empty()) {
            printUserRolesHelp(std::cerr);
            return commandFailure();
        }
        std::string head = args[0];
        std::string resolved;
        if (resolveSubcommand(std::vector<std::string>{"set"}, head, resolved) && resolved == "set") {
            args.erase(args.begin());
            if (takeHelpRequest(args)) {
                printUserRolesHelp(std::cout);
                return commandSuccess();
            }
            if (args.empty()) {
                std::cerr << "usage: user roles set USER R…\n";
                return commandFailure();
            }
            const std::string userName = args[0];
            if (!hasUser(userName)) {
                std::cerr << "user not found: " << userName << '\n';
                return commandFailure();
            }
            const std::vector<std::string> roles(args.begin() + 1, args.end());
            setRoles(userName, roles);
            std::cout << "roles updated for " << userName << '\n';
            return commandSuccess();
        }
        if (!hasUser(args[0])) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        for (const auto& role : getRoles(args[0])) {
            std::cout << "  role:" << role << '\n';
        }
        return commandSuccess();
    }
    if (sub == "role") {
        if (takeHelpRequest(args)) {
            printUserRoleHelp(std::cout);
            return commandSuccess();
        }
        if (args.size() < 3) {
            printUserRoleHelp(std::cerr);
            return commandFailure();
        }
        std::string op = args[0];
        if (!resolveSubcommand(kRoleSubCommands, op, op)) {
            return commandFailure();
        }
        const std::string& userName = args[1];
        const std::string& role = args[2];
        if (!hasUser(userName)) {
            std::cerr << "user not found: " << userName << '\n';
            return commandFailure();
        }
        if (op == "add") {
            addRole(userName, role);
        } else {
            removeRole(userName, role);
        }
        std::cout << "role " << op << ' ' << role << " for " << userName << '\n';
        return commandSuccess();
    }
    if (sub == "keys") {
        if (takeHelpRequest(args)) {
            printUserKeysHelp(std::cout);
            return commandSuccess();
        }
        if (args.empty()) {
            printUserKeysHelp(std::cerr);
            return commandFailure();
        }
        if (!hasUser(args[0])) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        for (const auto& key : getKeys(args[0])) {
            std::cout << "  " << key.id << " type=" << key.type << " name=" << key.name
                      << " enabled=" << (key.enabled ? "yes" : "no") << '\n';
        }
        return commandSuccess();
    }
    if (sub == "key") {
        if (takeHelpRequest(args)) {
            printUserKeyHelp(std::cout);
            return commandSuccess();
        }
        if (args.size() < 2) {
            printUserKeyHelp(std::cerr);
            return commandFailure();
        }
        std::string op = args[0];
        if (!resolveSubcommand(kKeySubCommands, op, op)) {
            return commandFailure();
        }
        if (op == "del") {
            if (takeHelpRequest(args)) {
                printUserKeyHelp(std::cout);
                return commandSuccess();
            }
            if (args.size() < 3) {
                printUserKeyHelp(std::cerr);
                return commandFailure();
            }
            removeKey(args[1], args[2]);
            std::cout << "removed key " << args[2] << " from " << args[1] << '\n';
            return commandSuccess();
        }
        if (takeHelpRequest(args)) {
            printUserKeyHelp(std::cout);
            return commandSuccess();
        }
        if (args.size() < 4) {
            printUserKeyHelp(std::cerr);
            return commandFailure();
        }
        const std::string userName = args[1];
        const std::string keyId = args[2];
        const std::string keyType = args[3];
        std::vector<std::string> rest(args.begin() + 4, args.end());
        if (!hasUser(userName)) {
            std::cerr << "user not found: " << userName << '\n';
            return commandFailure();
        }
        UserWriteOptions keyOpts;
        keyOpts.hashAlgo = "plain";
        std::string error;
        if (!parseUserWriteOptions(rest, keyOpts, false, false, error)) {
            std::cerr << error << '\n';
            return commandFailure();
        }
        std::string keyName = takeFlag(rest, "--name");
        if (keyName.empty()) {
            keyName = keyId;
        }

        UserAuthKey key;
        if (keyOpts.password.has_value()) {
            if (keyType == "password-hash" || keyType == "password-plain") {
                const std::string algo =
                    keyType == "password-plain" ? "plain" : keyOpts.hashAlgo;
                if (!isPasswordStorageAlgorithm(algo)) {
                    std::cerr << "unsupported hash algorithm: " << algo << '\n';
                    return commandFailure();
                }
                key = makePasswordKeyForAlgorithm(keyId, *keyOpts.password, algo);
            } else {
                std::cerr << "password flags require key type password-hash or password-plain\n";
                return commandFailure();
            }
            key.name = keyName;
        } else {
            key.id = keyId;
            key.type = keyType;
            key.name = keyName;
            key.enabled = true;
            key.createdAt = std::chrono::system_clock::now();
        }
        addKey(userName, key);
        std::cout << "added key " << keyId << " to " << userName << '\n';
        return commandSuccess();
    }

    std::cerr << "unknown user subcommand: " << sub << '\n';
    return commandFailure();
}

int UserStore::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }
    if (argsAreOnlyHelpFlags(args)) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }

    std::string head = shiftArg(args);
    if (head == "help" || head == "?") {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }

    std::string resolved;
    if (!resolveSubcommand(kStoreCommands, head, resolved)) {
        return commandFailure();
    }
    head = resolved;

    if (head == "path") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: path [-h]\n  Show user store file path and label.\n";
            return commandSuccess();
        }
        std::cout << "user store: " << storeLabel() << '\n';
        const auto path = storePath();
        if (!path.empty()) {
            std::cout << "  file: " << path << '\n';
        } else {
            std::cout << "  kind: in-memory\n";
        }
        return commandSuccess();
    }
    if (head == "reload") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: reload [-h]\n  Reload user store from disk.\n";
            return commandSuccess();
        }
        if (!canReloadFromDisk()) {
            std::cerr << "reload only applies to file store (-d/--store FILE)\n";
            return commandFailure();
        }
        reloadFromDisk();
        std::cout << "reloaded user store from " << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: save [-h]\n  Write user store to disk.\n";
            return commandSuccess();
        }
        if (!canSaveToDisk()) {
            std::cerr << "save only applies to file store (-d/--store FILE)\n";
            return commandFailure();
        }
        saveToDisk();
        std::cout << "saved user store to " << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "user") {
        return invokeUser(args);
    }

    args.insert(args.begin(), head);
    return invokeUser(args);
}

std::vector<std::string> UserStore::complete(const std::vector<std::string>& args,
                                             std::size_t index) const {
    const std::string cword = currentWord(args, index);

    const bool nestedUser = !args.empty() && args[0] == "user";
    if (nestedUser && index == 0) {
        return filterByPrefix(kStoreCommands, cword);
    }
    if (!nestedUser && index == 0) {
        std::vector<std::string> options = kStoreCommands;
        return filterByPrefix(options, cword);
    }

    std::size_t userIndex = index;
    std::string userSub;
    if (nestedUser) {
        userIndex = index > 0 ? index - 1 : 0;
        if (index == 1) {
            return filterByPrefix(kUserCommands, cword);
        }
        if (index >= 2) {
            userSub = args[1];
        }
    } else {
        if (index == 0) {
            return filterByPrefix(kUserCommands, cword);
        }
        userSub = args[0];
    }

    const std::vector<std::string> userCmdNeedingName = {
        "show", "del", "rm", "enable", "disable", "roles", "keys", "update", "check",
    };
    if (userIndex == 1) {
        if (userSub == "role" || userSub.rfind("ro", 0) == 0) {
            return filterByPrefix(kRoleSubCommands, cword);
        }
        if (userSub == "key" || userSub.rfind("ke", 0) == 0) {
            return filterByPrefix(kKeySubCommands, cword);
        }
        if (userSub == "roles" && nestedUser && index == 2) {
            return filterByPrefix(std::vector<std::string>{"set"}, cword);
        }
        const auto resolved = resolveCommandByPrefix(userCmdNeedingName, userSub);
        if (resolved.match == CommandMatch::Found || userSub == "add") {
            return filterByPrefix(listUsers(), cword);
        }
    }
    if (userIndex == 2) {
        if (userSub == "role" || userSub == "key") {
            return filterByPrefix(listUsers(), cword);
        }
        if (userSub == "roles") {
            return filterByPrefix(listUsers(), cword);
        }
    }
    if (userIndex >= 3 && (userSub == "key" || userSub.rfind("ke", 0) == 0)) {
        if (userIndex == 3) {
            return filterByPrefix(listUsers(), cword);
        }
        if (userIndex == 4) {
            return filterByPrefix(std::vector<std::string>{"password-hash", "password-plain"}, cword);
        }
    }
    return {};
}

int DecoratedUserStore::invoke(std::vector<std::string>& args) {
    return m_wrapped->invoke(args);
}

std::vector<std::string> DecoratedUserStore::complete(const std::vector<std::string>& args,
                                                      std::size_t index) const {
    return m_wrapped->complete(args, index);
}

std::string FileUserStore::storeLabel() const { return m_file.getPath(); }

std::filesystem::path FileUserStore::storePath() const {
    return std::filesystem::path(m_file.getLocalFile());
}

bool FileUserStore::canReloadFromDisk() const { return true; }

void FileUserStore::reloadFromDisk() { reload(); }

void FileUserStore::saveToDisk() { flush(); }

int FileUserStore::invoke(std::vector<std::string>& args) { return UserStore::invoke(args); }

std::vector<std::string> FileUserStore::complete(const std::vector<std::string>& args,
                                                 std::size_t index) const {
    return UserStore::complete(args, index);
}

std::string RegistryUserStore::storeLabel() const {
    return m_prefix.empty() ? "registry:/" : "registry:" + m_prefix;
}

std::filesystem::path RegistryUserStore::storePath() const {
    return std::filesystem::path(m_prefix);
}

bool RegistryUserStore::canReloadFromDisk() const { return true; }

void RegistryUserStore::reloadFromDisk() {
    m_registry->load();
    if (m_containers != nullptr) {
        m_containers->cacheClear();
    }
    loadFromRegistry();
}

void RegistryUserStore::saveToDisk() { flush(); }

int RegistryUserStore::invoke(std::vector<std::string>& args) { return UserStore::invoke(args); }

std::vector<std::string> RegistryUserStore::complete(const std::vector<std::string>& args,
                                                     std::size_t index) const {
    return UserStore::complete(args, index);
}

} // namespace bas::security
