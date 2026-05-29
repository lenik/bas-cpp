#include "identity_service.hpp"

#include "command_support.hpp"
#include "user_store.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printIdentityServiceHelp(std::ostream& out) {
    out << "identity service commands:\n"
           "  info                   show service id, type, credentials, realms\n"
           "  help                   show this help\n"
           "  store SUBCMD …         (store service) user-store commands\n";
}

void printIdentityServiceInfo(const IdentityService& service) {
    std::cout << "  id: " << service.id() << '\n';
    std::cout << "  type: " << service.identityType() << '\n';
    std::cout << "  auto-login: " << (service.canAutoLogin() ? "yes" : "no") << '\n';
    std::cout << "  credentials:";
    const auto credTypes = service.supportedCredentialTypes();
    if (credTypes.empty()) {
        std::cout << " (none)\n";
    } else {
        for (const auto& type : credTypes) {
            std::cout << ' ' << type;
        }
        std::cout << '\n';
    }
    std::cout << "  realm-types:";
    const auto realmTypes = service.supportedRealmTypes();
    if (realmTypes.empty()) {
        std::cout << " (any)\n";
    } else {
        for (const auto& type : realmTypes) {
            std::cout << ' ' << type;
        }
        std::cout << '\n';
    }
    const auto stores = service.getBackedStores();
    if (!stores.empty()) {
        std::cout << "  backed-stores: " << stores.size() << '\n';
    }
}

static const std::vector<std::string> kServiceCommands = {"info", "help", "store"};

} // namespace

int IdentityService::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printIdentityServiceInfo(*this);
        return commandSuccess();
    }

    const std::string sub = shiftArg(args);
    if (sub == "help" || sub == "?") {
        printIdentityServiceHelp(std::cout);
        return commandSuccess();
    }
    if (sub == "info") {
        printIdentityServiceInfo(*this);
        return commandSuccess();
    }

    std::cerr << "unknown identity service subcommand: " << sub << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> IdentityService::complete(const std::vector<std::string>& args,
                                                   std::size_t index) const {
    const std::string cword = currentWord(args, index);
    if (index == 0) {
        return filterByPrefix(kServiceCommands, cword);
    }
    return {};
}

int StoreIdentityService::invoke(std::vector<std::string>& args) {
    if (!args.empty() && args[0] == "store") {
        args.erase(args.begin());
        if (!m_userStore) {
            std::cerr << "user store not configured\n";
            return commandFailure();
        }
        return m_userStore->invoke(args);
    }
    return IdentityService::invoke(args);
}

std::vector<std::string> StoreIdentityService::complete(const std::vector<std::string>& args,
                                                          std::size_t index) const {
    if (!args.empty() && args[0] == "store") {
        if (!m_userStore) {
            return {};
        }
        std::vector<std::string> sub(args.begin() + 1, args.end());
        const std::size_t subIndex = index > 0 ? index - 1 : 0;
        return m_userStore->complete(sub, subIndex);
    }
    if (index == 0) {
        return filterByPrefix(kServiceCommands, currentWord(args, index));
    }
    return IdentityService::complete(args, index);
}

} // namespace bas::security
