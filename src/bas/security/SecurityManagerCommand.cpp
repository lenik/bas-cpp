#include "SecurityManager.hpp"

#include "ACList.hpp"
#include "CommandSupport.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printAcHelp(std::ostream& out) {
    out << "access control commands:\n"
           "  whoami                         list active identities\n"
           "  logout                         clear all active identities\n"
           "  logout-realm [@realm|NAME]     clear identities in a realm\n"
           "  check [@realm] PERMISSION      check permission\n"
           "  request [@realm] PERM [USER]   request permission (login if needed)\n"
           "  login [@realm] [USER]          interactive login\n"
           "  reload-creds                   reload credential file and restore logins\n"
           "  help                           show this help\n";
}

static std::vector<std::string> registeredRealmCompletions(const SecurityManager& ac,
                                                           const std::string& prefix) {
    std::vector<std::string> options;
    for (const auto& realm : ac.realms()) {
        if (!realm.name.empty()) {
            options.push_back("@" + realm.name);
        }
    }
    return filterByPrefix(options, prefix);
}

void printIdentities(const SecurityManager& ac) {
    const auto identities = ac.currentIdentities();
    if (identities.empty()) {
        std::cout << "no active identities\n";
        return;
    }
    for (const auto& id : identities) {
        std::cout << "  " << id.type << ':' << id.name;
        writeRealmSuffix(std::cout, id.realm);
        if (!id.displayName.empty() && id.displayName != id.name) {
            std::cout << " (" << id.displayName << ')';
        }
        std::cout << " via " << id.serviceId << '\n';
    }
}

void printMode(const std::string& verb, const std::string& permission, AccessEffect mode) {
    std::cout << verb << ' ' << permission << ": " << mode.str() << '\n';
}

static const std::vector<std::string> kAcCommands = {
    "whoami", "logout", "logout-realm", "check", "request", "login", "reload-creds", "help",
};

static const std::vector<std::string> kDemoPermissions = {
    "fab.order.view", "fab.order.modify", "fab.order.delete", "file.save", "file.*",
};

static const std::vector<std::string> kRealmCompletions = {
    "@global",
    "@device:",
    "@app:",
};

void peelLeadingRealm(std::vector<std::string>& args, Realm& parsed, bool& hasParsed) {
    hasParsed = false;
    if (!args.empty() && args[0].front() == '@') {
        if (auto realm = parseAtRealmToken(args[0])) {
            parsed = *realm;
            hasParsed = true;
            args.erase(args.begin());
        }
    }
}

} // namespace

void SecurityManager::setCommandDefaults(Realm defaultRealm, std::string defaultSubject) {
    m_cmdDefaultRealm = std::move(defaultRealm);
    m_cmdDefaultSubject = std::move(defaultSubject);
}

int SecurityManager::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printAcHelp(std::cout);
        return commandSuccess();
    }
    if (argsAreOnlyHelpFlags(args)) {
        printAcHelp(std::cout);
        return commandSuccess();
    }

    const std::string cmd = shiftArg(args);

    if (cmd == "help" || cmd == "?") {
        printAcHelp(std::cout);
        return commandSuccess();
    }
    if (cmd == "whoami") {
        printIdentities(*this);
        return commandSuccess();
    }
    if (cmd == "logout") {
        logoutAll();
        std::cout << "logged out all identities (auto-login suppressed until next login)\n";
        return commandSuccess();
    }
    if (cmd == "logout-realm") {
        Realm realm = m_cmdDefaultRealm;
        if (!args.empty()) {
            if (auto parsed = parseAtRealmToken(args[0])) {
                realm = mergeRealm(realm, *parsed);
            } else {
                realm.name = args[0];
            }
        }
        if (realm.empty()) {
            std::cerr << "usage: logout-realm [@realm|NAME]\n";
            return commandFailure();
        }
        logoutRealm(realm);
        std::cout << "logged out identities in realm " << realm.displayLabel() << '\n';
        return commandSuccess();
    }
    if (cmd == "reload-creds") {
        if (!m_credentialManager->canReloadCredentials()) {
            std::cerr << "no file credential store configured\n";
            return commandFailure();
        }
        m_credentialManager->reloadCredentials();
        std::cout << "reloaded " << m_credentialManager->credentialPersistedCount()
                  << " credential(s) from " << m_credentialManager->credentialPath() << '\n';
        AccessRequestOptions options;
        activateCachedCredentials(options);
        printIdentities(*this);
        return commandSuccess();
    }

    Realm overlay;
    bool hasOverlay = false;
    peelLeadingRealm(args, overlay, hasOverlay);
    Realm realm = m_cmdDefaultRealm;
    if (hasOverlay) {
        realm = mergeRealm(realm, overlay);
    }
    realm = resolveRealmHint(realm);

    if (cmd == "check") {
        if (args.empty()) {
            std::cerr << "usage: check [@realm] PERMISSION\n";
            return commandFailure();
        }
        AccessRequestOptions options;
        options.realmHint = realm;
        printMode("check", args[0], checkPermission(Permission::parse(args[0]), options));
        return commandSuccess();
    }
    if (cmd == "request") {
        if (args.empty()) {
            std::cerr << "usage: request [@realm] PERMISSION [USER]\n";
            return commandFailure();
        }
        AccessRequestOptions options;
        options.allowGuiInteraction = true;
        options.allowConsoleInteraction = true;
        options.allowAutoLogin = !autoLoginSuppressed();
        options.realmHint = realm;
        options.reason = "acdemo request";
        if (args.size() >= 2) {
            options.nameHint = args[1];
        } else if (!m_cmdDefaultSubject.empty()) {
            options.nameHint = m_cmdDefaultSubject;
        }
        printMode("request", args[0], requestPermission(Permission::parse(args[0]), options));
        printIdentities(*this);
        return commandSuccess();
    }
    if (cmd == "login") {
        AccessRequestOptions options;
        options.allowGuiInteraction = true;
        options.allowConsoleInteraction = true;
        options.realmHint = realm;
        options.reason = "acdemo login";
        if (!args.empty()) {
            options.nameHint = args[0];
        } else if (!m_cmdDefaultSubject.empty()) {
            options.nameHint = m_cmdDefaultSubject;
        }
        if (login(options)) {
            std::cout << "login ok";
            if (!realm.empty()) {
                std::cout << " realm=" << realm.displayLabel();
            }
            std::cout << '\n';
            printIdentities(*this);
            return commandSuccess();
        }
        std::cerr << "login failed\n";
        return commandFailure();
    }

    std::cerr << "unknown command: " << cmd << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> SecurityManager::complete(const std::vector<std::string>& args,
                                                    std::size_t index) const {
    const std::string cword = currentWord(args, index);
    if (index == 0) {
        return filterByPrefix(kAcCommands, cword);
    }
    if (args.empty()) {
        return {};
    }
    const std::string& cmd = args[0];
    if (cmd == "check") {
        if (index == 1) {
            std::vector<std::string> out = registeredRealmCompletions(*this, cword);
            const auto generic = filterByPrefix(kRealmCompletions, cword);
            out.insert(out.end(), generic.begin(), generic.end());
            const auto perms = filterByPrefix(kDemoPermissions, cword);
            out.insert(out.end(), perms.begin(), perms.end());
            return out;
        }
        if (index == 2) {
            return filterByPrefix(kDemoPermissions, cword);
        }
    }
    if (cmd == "request") {
        if (index == 1) {
            std::vector<std::string> out = registeredRealmCompletions(*this, cword);
            const auto generic = filterByPrefix(kRealmCompletions, cword);
            out.insert(out.end(), generic.begin(), generic.end());
            const auto perms = filterByPrefix(kDemoPermissions, cword);
            out.insert(out.end(), perms.begin(), perms.end());
            return out;
        }
        if (index == 2) {
            return filterByPrefix(kDemoPermissions, cword);
        }
    }
    if (cmd == "login") {
        if (index == 1) {
            std::vector<std::string> out = registeredRealmCompletions(*this, cword);
            const auto generic = filterByPrefix(kRealmCompletions, cword);
            out.insert(out.end(), generic.begin(), generic.end());
            return out;
        }
    }
    if (cmd == "logout-realm" && index == 1) {
        std::vector<std::string> out = registeredRealmCompletions(*this, cword);
        const auto generic = filterByPrefix(kRealmCompletions, cword);
        out.insert(out.end(), generic.begin(), generic.end());
        return out;
    }
    return {};
}

} // namespace bas::security
