#include "PolicyStore.hpp"

#include "ACList.hpp"
#include "CommandSupport.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printAclHelp(std::ostream& out) {
    out << "acl commands:\n"
           "  list                   list ACL grants\n"
           "  clear                  remove all grants\n"
           "  path                   show ACL file path\n"
           "  reload                 reload file ACL from disk\n"
           "  save                   write file ACL to disk\n"
           "  help                   show this help\n"
           "  -h, --help             show this help\n";
}

void printAclGrants(const PolicyStore& store) {
    const auto& grants = store.grants();
    if (grants.empty()) {
        std::cout << "no ACL grants\n";
        return;
    }
    for (const auto& grant : grants) {
        std::cout << "  " << grant.identity.type << ':' << grant.identity.name;
        writeRealmSuffix(std::cout, grant.identity.realm);
        std::cout << "  " << grant.permission.toString() << " -> "
                  << grant.effect.str() << '\n';
    }
}

static const std::vector<std::string> kAclCommands = {
    "list", "clear", "path", "reload", "save", "help",
};

} // namespace

int PolicyStore::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printAclGrants(*this);
        return commandSuccess();
    }
    if (argsAreOnlyHelpFlags(args)) {
        printAclHelp(std::cout);
        return commandSuccess();
    }

    const std::string head = shiftArg(args);

    if (head == "help" || head == "?") {
        printAclHelp(std::cout);
        return commandSuccess();
    }
    if (head == "list") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: list [-h]\n  List ACL grants.\n";
            return commandSuccess();
        }
        printAclGrants(*this);
        return commandSuccess();
    }
    if (head == "clear") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: clear [-h]\n  Remove all ACL grants.\n";
            return commandSuccess();
        }
        clear();
        std::cout << "cleared ACL\n";
        return commandSuccess();
    }
    if (head == "path") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: path [-h]\n  Show ACL file path.\n";
            return commandSuccess();
        }
        const auto path = storePath();
        if (path.empty()) {
            std::cout << "acl: in-memory\n";
        } else {
            std::cout << "acl file: " << path << '\n';
        }
        return commandSuccess();
    }
    if (head == "reload") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: reload [-h]\n  Reload file ACL from disk.\n";
            return commandSuccess();
        }
        if (!canPersistToDisk()) {
            std::cerr << "reload only applies to file ACL (-a/--acl FILE)\n";
            return commandFailure();
        }
        reloadFromDisk();
        std::cout << "reloaded ACL from " << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: save [-h]\n  Write file ACL to disk.\n";
            return commandSuccess();
        }
        if (!canPersistToDisk()) {
            std::cerr << "save only applies to file ACL (-a/--acl FILE)\n";
            return commandFailure();
        }
        persistToDisk();
        std::cout << "saved ACL to " << storePath() << '\n';
        return commandSuccess();
    }

    if (takeHelpRequest(args)) {
        printAclHelp(std::cout);
        return commandSuccess();
    }

    std::cerr << "unknown acl command: " << head << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> PolicyStore::complete(const std::vector<std::string>& args,
                                               std::size_t index) const {
    return filterByPrefix(kAclCommands, currentWord(args, index));
}

} // namespace bas::security
