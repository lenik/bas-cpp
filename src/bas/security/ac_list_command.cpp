#include "ac_list.hpp"

#include "command_support.hpp"
#include "json.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printAclHelp(std::ostream& out) {
    out << "acl commands:\n"
           "  list                   list ACL entries\n"
           "  clear                  remove all entries\n"
           "  path                   show ACL file path\n"
           "  reload                 reload file ACL from disk\n"
           "  save                   write file ACL to disk\n"
           "  help                   show this help\n";
}

void printAclEntries(const ACList& acl) {
    const auto& entries = acl.entries();
    if (entries.empty()) {
        std::cout << "no ACL entries\n";
        return;
    }
    for (const auto& entry : entries) {
        std::cout << "  " << entry.identity.type << ':' << entry.identity.name;
        writeRealmSuffix(std::cout, entry.identity.realm);
        std::cout << "  " << entry.rule.permission << " -> " << acModeToString(entry.rule.mode)
                  << '\n';
    }
}

static const std::vector<std::string> kAclCommands = {
    "list", "clear", "path", "reload", "save", "help",
};

} // namespace

int ACList::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printAclEntries(*this);
        return commandSuccess();
    }

    const std::string head = shiftArg(args);

    if (head == "help" || head == "?") {
        printAclHelp(std::cout);
        return commandSuccess();
    }
    if (head == "list") {
        printAclEntries(*this);
        return commandSuccess();
    }
    if (head == "clear") {
        clear();
        std::cout << "cleared ACL\n";
        return commandSuccess();
    }
    if (head == "path") {
        const auto path = aclPath();
        if (path.empty()) {
            std::cout << "acl: in-memory\n";
        } else {
            std::cout << "acl file: " << path << '\n';
        }
        return commandSuccess();
    }
    if (head == "reload") {
        if (!canPersistToDisk()) {
            std::cerr << "reload only applies to file ACL (-a/--acl FILE)\n";
            return commandFailure();
        }
        reloadFromDisk();
        std::cout << "reloaded ACL from " << aclPath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
        if (!canPersistToDisk()) {
            std::cerr << "save only applies to file ACL (-a/--acl FILE)\n";
            return commandFailure();
        }
        persistToDisk();
        std::cout << "saved ACL to " << aclPath() << '\n';
        return commandSuccess();
    }

    std::cerr << "unknown acl command: " << head << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> ACList::complete(const std::vector<std::string>& args,
                                          std::size_t index) const {
    return filterByPrefix(kAclCommands, currentWord(args, index));
}

int DecoratedACList::invoke(std::vector<std::string>& args) {
    return m_wrapped->invoke(args);
}

std::vector<std::string> DecoratedACList::complete(const std::vector<std::string>& args,
                                                   std::size_t index) const {
    return m_wrapped->complete(args, index);
}

std::filesystem::path FileACList::aclPath() const {
    return std::filesystem::path(m_file.getLocalFile());
}

bool FileACList::canPersistToDisk() const { return true; }

void FileACList::reloadFromDisk() { reload(); }

void FileACList::persistToDisk() { flush(); }

int FileACList::invoke(std::vector<std::string>& args) { return ACList::invoke(args); }

std::vector<std::string> FileACList::complete(const std::vector<std::string>& args,
                                              std::size_t index) const {
    return ACList::complete(args, index);
}

} // namespace bas::security
