#include "user_store.hpp"

#include "command_support.hpp"

#include <boost/json.hpp>

#include <chrono>
#include <iostream>

namespace bas::security {

namespace {

void printUserStoreHelp(std::ostream& out) {
    out << "user store commands:\n"
           "  user list                       list usernames\n"
           "  user show USER                  show profile, roles, keys\n"
           "  user add USER [options]         add user record\n"
           "      --display NAME  --email EMAIL  --password P\n"
           "      --role ROLE     (repeatable)\n"
           "  user del USER                   remove user\n"
           "  user enable USER | user disable USER\n"
           "  user roles USER                 list roles\n"
           "  user roles set USER ROLE…       replace roles\n"
           "  user role add USER ROLE | user role del USER ROLE\n"
           "  user keys USER                  list auth keys\n"
           "  user key add USER ID TYPE [--password P] [--name LABEL]\n"
           "  user key del USER KEY_ID\n"
           "  path                            show store location\n"
           "  reload                          reload file store from disk\n"
           "  save                            write file store to disk\n"
           "  help                            show this help\n"
           "  (legacy: omit 'user' prefix, e.g. list, add, del)\n";
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
    std::cout << "  enabled: " << (p.enabled ? "yes" : "no") << '\n';
    std::cout << "  roles:";
    if (record.roles.empty()) {
        std::cout << " (none)\n";
    } else {
        for (const auto& role : record.roles) {
            std::cout << ' ' << role.name;
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
            if (key.type == "password-hash" && key.data.if_contains("hash")) {
                std::cout << " hash=(set)";
            }
            std::cout << '\n';
        }
    }
}

static const std::vector<std::string> kUserTopCommands = {
    "list", "show", "add", "del", "rm", "enable", "disable", "roles", "role", "keys", "key", "help",
};

static const std::vector<std::string> kStoreTopCommands = {
    "user", "help", "path", "reload", "save", "list", "show", "add", "del",
};

} // namespace

int UserStore::invokeUser(std::vector<std::string>& args) {
    if (args.empty()) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }

    const std::string sub = shiftArg(args);

    if (sub == "help" || sub == "?") {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }
    if (sub == "list") {
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
        if (args.empty()) {
            std::cerr << "usage: user show USER\n";
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
        if (args.empty()) {
            std::cerr << "usage: user add USER [--display N] [--email E] [--password P] [--role R]…\n";
            return commandFailure();
        }
        const std::string userName = args[0];
        args.erase(args.begin());
        if (hasUser(userName)) {
            std::cerr << "user already exists: " << userName << '\n';
            return commandFailure();
        }
        const std::string display = takeFlag(args, "--display");
        const std::string email = takeFlag(args, "--email");
        const std::string password = takeFlag(args, "--password");
        std::vector<IdentityRef> roles;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--role" && i + 1 < args.size()) {
                roles.push_back(roleRef(Realm::GLOBAL, args[i + 1]));
                args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                            args.begin() + static_cast<std::ptrdiff_t>(i + 2));
                --i;
            }
        }
        if (roles.empty()) {
            roles.push_back(roleRef(Realm::GLOBAL, "operator"));
        }

        UserRecord record;
        record.profile.name = userName;
        record.profile.displayName = display.empty() ? userName : display;
        record.profile.email = email;
        record.profile.enabled = true;
        record.roles = roles;
        if (!password.empty()) {
            UserAuthKey key;
            key.id = "pwd-main";
            key.type = "password-hash";
            key.name = "Main Password";
            key.enabled = true;
            key.createdAt = std::chrono::system_clock::now();
            key.data["algorithm"] = boost::json::value("demo-plain");
            key.data["hash"] = boost::json::value(password);
            record.keys.push_back(std::move(key));
        }
        addUser(record);
        std::cout << "added user " << userName << '\n';
        return commandSuccess();
    }
    if (sub == "del" || sub == "rm") {
        if (args.empty()) {
            std::cerr << "usage: user del USER\n";
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
        if (args.empty()) {
            std::cerr << "usage: user " << sub << " USER\n";
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
        if (args.empty()) {
            std::cerr << "usage: user roles USER | user roles set USER R…\n";
            return commandFailure();
        }
        if (args.size() >= 2 && args[0] == "set") {
            const std::string userName = args[1];
            if (!hasUser(userName)) {
                std::cerr << "user not found: " << userName << '\n';
                return commandFailure();
            }
            std::vector<IdentityRef> roles;
            for (const auto& roleName : std::vector<std::string>(args.begin() + 2, args.end())) {
                roles.push_back(roleRef(Realm::GLOBAL, roleName));
            }
            setRoles(userName, roles);
            std::cout << "roles updated for " << userName << '\n';
            return commandSuccess();
        }
        if (!hasUser(args[0])) {
            std::cerr << "user not found: " << args[0] << '\n';
            return commandFailure();
        }
        for (const auto& role : getRoles(args[0])) {
            std::cout << "  " << role.type << ':' << role.name;
            writeRealmSuffix(std::cout, role.realm);
            std::cout << '\n';
        }
        return commandSuccess();
    }
    if (sub == "role") {
        if (args.size() < 3 || (args[0] != "add" && args[0] != "del")) {
            std::cerr << "usage: user role add USER ROLE | user role del USER ROLE\n";
            return commandFailure();
        }
        const std::string& op = args[0];
        const std::string& userName = args[1];
        const std::string& role = args[2];
        if (!hasUser(userName)) {
            std::cerr << "user not found: " << userName << '\n';
            return commandFailure();
        }
        IdentityRef roleIdentity = roleRef(Realm::GLOBAL, role);
        if (op == "add") {
            addRole(userName, roleIdentity);
        } else {
            removeRole(userName, roleIdentity);
        }
        std::cout << "role " << op << ' ' << role << " for " << userName << '\n';
        return commandSuccess();
    }
    if (sub == "keys") {
        if (args.empty()) {
            std::cerr << "usage: user keys USER\n";
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
        if (args.size() < 2 || (args[0] != "add" && args[0] != "del")) {
            std::cerr << "usage: user key add USER ID TYPE [--password P] [--name N]\n"
                         "       user key del USER ID\n";
            return commandFailure();
        }
        const std::string& op = args[0];
        if (op == "del") {
            if (args.size() < 3) {
                std::cerr << "usage: user key del USER ID\n";
                return commandFailure();
            }
            removeKey(args[1], args[2]);
            std::cout << "removed key " << args[2] << " from " << args[1] << '\n';
            return commandSuccess();
        }
        if (args.size() < 4) {
            std::cerr << "usage: user key add USER ID TYPE [--password P]\n";
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
        UserAuthKey key;
        key.id = keyId;
        key.type = keyType;
        key.name = takeFlag(rest, "--name");
        if (key.name.empty()) {
            key.name = keyId;
        }
        key.enabled = true;
        key.createdAt = std::chrono::system_clock::now();
        const std::string password = takeFlag(rest, "--password");
        if (keyType == "password-hash" && !password.empty()) {
            key.data["algorithm"] = boost::json::value("demo-plain");
            key.data["hash"] = boost::json::value(password);
        }
        addKey(userName, key);
        std::cout << "added key " << keyId << " to " << userName << '\n';
        return commandSuccess();
    }

    std::cerr << "unknown user subcommand: " << sub << " (try: user help)\n";
    return commandFailure();
}

int UserStore::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }

    const std::string head = shiftArg(args);

    if (head == "help" || head == "?") {
        printUserStoreHelp(std::cout);
        return commandSuccess();
    }
    if (head == "path") {
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
        if (!canReloadFromDisk()) {
            std::cerr << "reload only applies to file store (-d/--store FILE)\n";
            return commandFailure();
        }
        reloadFromDisk();
        std::cout << "reloaded user store from " << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
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
        return filterByPrefix(kStoreTopCommands, cword);
    }
    if (!nestedUser && index == 0) {
        std::vector<std::string> options = kStoreTopCommands;
        options.insert(options.end(), kUserTopCommands.begin(), kUserTopCommands.end());
        return filterByPrefix(options, cword);
    }

    std::size_t userIndex = index;
    std::string userSub;
    if (nestedUser) {
        userIndex = index > 0 ? index - 1 : 0;
        if (index == 1) {
            return filterByPrefix(kUserTopCommands, cword);
        }
        if (index >= 2) {
            userSub = args[1];
        }
    } else {
        if (index == 0) {
            return filterByPrefix(kUserTopCommands, cword);
        }
        userSub = args[0];
    }

    const std::vector<std::string> userCmdNeedingName = {
        "show", "del", "rm", "enable", "disable", "roles", "keys",
    };
    if (userIndex == 1) {
        if (userSub == "role") {
            return filterByPrefix(std::vector<std::string>{"add", "del"}, cword);
        }
        if (userSub == "key") {
            return filterByPrefix(std::vector<std::string>{"add", "del"}, cword);
        }
        if (userSub == "roles" && nestedUser && index == 2 && args.size() > 2 && args[2] == "set") {
            return filterByPrefix(listUsers(), cword);
        }
        if (std::find(userCmdNeedingName.begin(), userCmdNeedingName.end(), userSub) !=
            userCmdNeedingName.end()) {
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
    if (userIndex >= 3 && userSub == "key") {
        if (userIndex == 3) {
            return filterByPrefix(listUsers(), cword);
        }
        if (userIndex == 4) {
            return filterByPrefix(std::vector<std::string>{"password-hash"}, cword);
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

std::string DemoUserStore::storeLabel() const { return "demo"; }

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

} // namespace bas::security
