#include "ACList.hpp"

#include "CommandSupport.hpp"

#include "bas/script/json.hpp"

#include <iostream>

namespace bas::security {

void ACEntry::jsonIn(const boost::json::object& o, const JsonFormOptions& /*opts*/) {
    permission = Permission::parse(boost::json::get_string(o, "permission", ""));
    effect = AccessEffect::fromString(boost::json::get_string(o, "mode", ""));
}

void ACEntry::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) const {
    o["permission"] = permission.toString();
    o["mode"] = effect.str();
}

namespace {

void printAclListHelp(std::ostream& out) {
    out << "aclist commands:\n"
           "  list                   list ACE rows\n"
           "  help                   show this help\n"
           "  -h, --help             show this help\n";
}

void printEntries(const ACList& acl) {
    if (acl.entries.empty()) {
        std::cout << "no ACE rows\n";
        return;
    }
    for (const auto& entry : acl.entries) {
        std::cout << "  " << entry.permission.toString() << " -> " << entry.effect.str() << '\n';
    }
}

static const std::vector<std::string> kAclListCommands = {
    "list",
    "help",
};

} // namespace

void ACList::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    id = boost::json::get_string(o, "id", "");
    entries.clear();
    if (auto* rows = o.if_contains("entries")) {
        if (!rows->is_array()) {
            return;
        }
        for (auto& item : rows->as_array()) {
            if (!item.is_object()) {
                continue;
            }
            ACEntry entry;
            entry.jsonIn(item.as_object(), opts);
            entries.push_back(entry);
        }
    }
}

void ACList::jsonOut(boost::json::object& o, const JsonFormOptions& opts) {
    if (!id.empty()) {
        o["id"] = id;
    }
    boost::json::array rows;
    for (const auto& entry : entries) {
        boost::json::object item;
        entry.jsonOut(item, opts);
        rows.push_back(item);
    }
    o["entries"] = rows;
}

int ACList::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        if (!id.empty()) {
            std::cout << id << ":\n";
        }
        printEntries(*this);
        return commandSuccess();
    }
    if (argsAreOnlyHelpFlags(args)) {
        printAclListHelp(std::cout);
        return commandSuccess();
    }

    const std::string head = shiftArg(args);

    if (head == "help" || head == "?") {
        printAclListHelp(std::cout);
        return commandSuccess();
    }
    if (head == "list") {
        if (takeHelpRequest(args)) {
            std::cout << "usage: list [-h]\n  List ACE rows.\n";
            return commandSuccess();
        }
        printEntries(*this);
        return commandSuccess();
    }

    if (takeHelpRequest(args)) {
        printAclListHelp(std::cout);
        return commandSuccess();
    }

    std::cerr << "unknown aclist command: " << head << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> ACList::complete(const std::vector<std::string>& args,
                                          std::size_t index) const {
    return filterByPrefix(kAclListCommands, currentWord(args, index));
}

} // namespace bas::security
