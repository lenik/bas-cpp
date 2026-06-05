#include "IdentityRegistry.hpp"

#include "CommandSupport.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printRegistryHelp(std::ostream& out) {
    out << "identity registry commands:\n"
           "  list                   list registered identity services\n"
           "  realms                 list realm → service bindings\n"
           "  service ID [SUBCMD …]  run commands on a service by id\n"
           "  help                   show this help\n";
}

void printRegistryServices(const IdentityRegistry& registry) {
    const auto services = registry.list();
    if (services.empty()) {
        std::cout << "no identity services registered\n";
        return;
    }
    for (const auto& service : services) {
        if (!service) {
            continue;
        }
        std::cout << "  " << service->id() << " type=" << service->identityType();
        if (service->canAutoLogin()) {
            std::cout << " [auto-login]";
        }
        std::cout << '\n';
    }
}

void printRegistryRealms(const IdentityRegistry& registry) {
    const auto& realms = registry.realms();
    if (realms.empty()) {
        std::cout << "no realms registered\n";
        return;
    }
    for (const auto& realm : realms) {
        const auto service = registry.load(realm);
        std::cout << "  " << realm.displayLabel() << " ->";
        if (!service) {
            std::cout << " (no service)\n";
        } else {
            std::cout << ' ' << service->id() << '\n';
        }
    }
}

static const std::vector<std::string> kRegistryCommands = {"list", "realms", "service", "help"};

std::vector<std::string> serviceIds(const IdentityRegistry& registry) {
    std::vector<std::string> ids;
    for (const auto& service : registry.list()) {
        if (service) {
            ids.push_back(service->id());
        }
    }
    return ids;
}

} // namespace

int IdentityRegistry::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printRegistryHelp(std::cout);
        return commandSuccess();
    }

    const std::string sub = shiftArg(args);

    if (sub == "help" || sub == "?") {
        printRegistryHelp(std::cout);
        return commandSuccess();
    }
    if (sub == "list") {
        printRegistryServices(*this);
        return commandSuccess();
    }
    if (sub == "realms") {
        printRegistryRealms(*this);
        return commandSuccess();
    }
    if (sub == "service") {
        if (args.empty()) {
            std::cerr << "usage: service ID [SUBCMD …]\n";
            return commandFailure();
        }
        const std::string serviceId = shiftArg(args);
        auto service = findById(serviceId);
        if (!service) {
            std::cerr << "identity service not found: " << serviceId << '\n';
            return commandFailure();
        }
        return service->invoke(args);
    }

    std::cerr << "unknown registry subcommand: " << sub << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> IdentityRegistry::complete(const std::vector<std::string>& args,
                                                    std::size_t index) const {
    const std::string cword = currentWord(args, index);
    if (index == 0) {
        return filterByPrefix(kRegistryCommands, cword);
    }
    if (args.empty()) {
        return {};
    }
    if (args[0] == "service") {
        if (index == 1) {
            return filterByPrefix(serviceIds(*this), cword);
        }
        if (index >= 2) {
            const auto service = findById(args[1]);
            if (!service) {
                return {};
            }
            std::vector<std::string> sub(args.begin() + 2, args.end());
            const std::size_t subIndex = index > 2 ? index - 2 : 0;
            return service->complete(sub, subIndex);
        }
    }
    return {};
}

} // namespace bas::security
