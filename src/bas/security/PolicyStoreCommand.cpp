#include "PolicyStore.hpp"

#include "ACList.hpp"
#include "CommandSupport.hpp"

#include <bas/locale/i18n.h>

#include <iostream>

namespace bas::security {

namespace {

void printAclHelp(std::ostream& out) {
    out << _("acl commands:\n"
             "  list                   list ACL grants\n"
             "  clear                  remove all grants\n"
             "  path                   show ACL file path\n"
             "  reload                 reload file ACL from disk\n"
             "  save                   write file ACL to disk\n"
             "  allow IDENTITY PERM    add allow grant\n"
             "  deny IDENTITY PERM     add deny grant\n"
             "  revoke IDENTITY PERM   remove matching grant\n"
             "  help                   show this help\n"
             "  -h, --help             show this help\n"
             "  IDENTITY: name | user:name | role:name\n"
             "  PERM: action=read;resource=/path/**\n");
}

IdentityRef parseIdentityToken(const std::string& token) {
    if (token.rfind("role:", 0) == 0)
        return IdentityRef{"role", Realm{}, token.substr(5)};
    if (token.rfind("user:", 0) == 0)
        return IdentityRef{"user", Realm{}, token.substr(5)};
    return IdentityRef{"user", Realm{}, token};
}

Permission parsePermissionToken(const std::string& token) {
    // Allow shorthand action:resource → action=…;resource=…
    if (token.find('=') == std::string::npos) {
        const auto colon = token.find(':');
        if (colon != std::string::npos && colon > 0) {
            Permission p;
            p.action = token.substr(0, colon);
            p.resource = token.substr(colon + 1);
            return p;
        }
    }
    return Permission::parse(token);
}

void printAclGrants(const PolicyStore& store) {
    const auto& grants = store.grants();
    if (grants.empty()) {
        std::cout << _("no ACL grants\n");
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
    "list", "clear", "path", "reload", "save", "allow", "deny", "revoke", "grant", "help",
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
            std::cout << _("usage: list [-h]\n  List ACL grants.\n");
            return commandSuccess();
        }
        printAclGrants(*this);
        return commandSuccess();
    }
    if (head == "clear") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: clear [-h]\n  Remove all ACL grants.\n");
            return commandSuccess();
        }
        clear();
        std::cout << _("cleared ACL\n");
        return commandSuccess();
    }
    if (head == "path") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: path [-h]\n  Show ACL file path.\n");
            return commandSuccess();
        }
        const auto path = storePath();
        if (path.empty()) {
            std::cout << _("acl: in-memory\n");
        } else {
            std::cout << _("acl file:") << path << '\n';
        }
        return commandSuccess();
    }
    if (head == "reload") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: reload [-h]\n  Reload file ACL from disk.\n");
            return commandSuccess();
        }
        if (!canPersistToDisk()) {
            std::cerr << _("reload only applies to file ACL (-a/--acl FILE)\n");
            return commandFailure();
        }
        reloadFromDisk();
        std::cout << _("reloaded ACL from") << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: save [-h]\n  Write file ACL to disk.\n");
            return commandSuccess();
        }
        if (!canPersistToDisk()) {
            std::cerr << _("save only applies to file ACL (-a/--acl FILE)\n");
            return commandFailure();
        }
        persistToDisk();
        std::cout << _("saved ACL to") << storePath() << '\n';
        return commandSuccess();
    }
    if (head == "allow" || head == "deny" || head == "grant") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: allow|deny IDENTITY PERMISSION\n"
                           "       grant IDENTITY PERMISSION [allow|deny]\n");
            return commandSuccess();
        }
        AccessEffect effect = head == "deny" ? AccessEffect::Deny : AccessEffect::Allow;
        if (head == "grant") {
            if (args.size() < 2) {
                std::cerr << _("usage: grant IDENTITY PERMISSION [allow|deny]\n");
                return commandFailure();
            }
            if (args.size() >= 3) {
                const std::string& mode = args[2];
                if (mode == "deny" || mode == "Deny")
                    effect = AccessEffect::Deny;
                else if (mode == "allow" || mode == "Allow")
                    effect = AccessEffect::Allow;
                else {
                    std::cerr << _("unknown effect:") << ' ' << mode << '\n';
                    return commandFailure();
                }
            }
        } else if (args.size() < 2) {
            std::cerr << _("usage:") << ' ' << head << ' ' << _("IDENTITY PERMISSION\n");
            return commandFailure();
        }
        const IdentityRef id = parseIdentityToken(args[0]);
        const Permission perm = parsePermissionToken(args[1]);
        addGrant(makeAccessGrant(id, perm, effect));
        std::cout << head << ' ' << id.type << ':' << id.name << "  " << perm.toString() << " -> "
                  << effect.str() << '\n';
        return commandSuccess();
    }
    if (head == "revoke" || head == "ungrant") {
        if (takeHelpRequest(args)) {
            std::cout << _("usage: revoke IDENTITY PERMISSION\n");
            return commandSuccess();
        }
        if (args.size() < 2) {
            std::cerr << _("usage: revoke IDENTITY PERMISSION\n");
            return commandFailure();
        }
        const IdentityRef id = parseIdentityToken(args[0]);
        const Permission perm = parsePermissionToken(args[1]);
        // Remove both allow and deny matches on this identity+permission.
        removeGrant(makeAccessGrant(id, perm, AccessEffect::Allow));
        removeGrant(makeAccessGrant(id, perm, AccessEffect::Deny));
        std::cout << _("revoked") << ' ' << id.type << ':' << id.name << "  " << perm.toString()
                  << '\n';
        return commandSuccess();
    }

    if (takeHelpRequest(args)) {
        printAclHelp(std::cout);
        return commandSuccess();
    }

    std::cerr << _("unknown acl command:") << head << _("(try: help)\n");
    return commandFailure();
}

std::vector<std::string> PolicyStore::complete(const std::vector<std::string>& args,
                                               std::size_t index) const {
    return filterByPrefix(kAclCommands, currentWord(args, index));
}

} // namespace bas::security
