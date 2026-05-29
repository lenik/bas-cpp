#include "credential.hpp"

#include "command_support.hpp"

#include <iostream>

namespace bas::security {

namespace {

void printCredentialHelp(std::ostream& out) {
    out << "credential commands:\n"
           "  list [-v]              list cached credentials (no secrets)\n"
           "  del STORE ID           remove credential by ref\n"
           "  clear                  remove all credentials\n"
           "  path                   show credential file path\n"
           "  reload                 reload from disk\n"
           "  save                   write credentials to disk\n"
           "  help                   show this help\n";
}

void printCredentials(const CredentialManager& credentials, bool verbose) {
    CredentialRequest req;
    const auto infos = credentials.list(req);
    if (infos.empty()) {
        std::cout << "no cached credentials\n";
        return;
    }
    for (const auto& info : infos) {
        std::cout << "  [" << info.ref.store << "] " << info.ref.id << " type=" << info.meta.type
                  << " subject=" << info.meta.subjectHint << " service=" << info.meta.serviceHint;
        writeRealmSuffix(std::cout, info.meta.realm);
        if (!info.meta.name.empty()) {
            std::cout << " label=" << info.meta.name;
        }
        std::cout << '\n';
        if (verbose) {
            std::cout << "    (use login/request to verify; secrets not printed)\n";
        }
    }
}

static const std::vector<std::string> kCredCommands = {
    "list", "del", "clear", "path", "reload", "save", "help",
};

} // namespace

int CredentialManager::invoke(std::vector<std::string>& args) {
    if (args.empty()) {
        printCredentials(*this, false);
        return commandSuccess();
    }

    const std::string head = shiftArg(args);

    if (head == "help" || head == "?") {
        printCredentialHelp(std::cout);
        return commandSuccess();
    }
    if (head == "list" || head == "-v") {
        const bool verbose = head == "-v" || hasFlag(args, "-v");
        printCredentials(*this, verbose);
        return commandSuccess();
    }
    if (head == "path") {
        const auto path = credentialPath();
        if (path.empty()) {
            std::cout << "credential store: in-memory\n";
        } else {
            std::cout << "credential file: " << path << '\n';
        }
        return commandSuccess();
    }
    if (head == "reload") {
        if (!canReloadCredentials()) {
            std::cerr << "reload only applies to file credential store\n";
            return commandFailure();
        }
        reloadCredentials();
        std::cout << "reloaded " << credentialPersistedCount() << " credential(s) from "
                  << credentialPath() << '\n';
        return commandSuccess();
    }
    if (head == "save") {
        if (!canSaveCredentials()) {
            std::cerr << "save only applies to file credential store\n";
            return commandFailure();
        }
        saveCredentials();
        std::cout << "saved credentials to " << credentialPath() << '\n';
        return commandSuccess();
    }
    if (head == "clear") {
        clear();
        std::cout << "cleared all credentials\n";
        return commandSuccess();
    }
    if (head == "del") {
        if (args.size() < 2) {
            std::cerr << "usage: del STORE ID\n";
            return commandFailure();
        }
        CredentialRef ref;
        ref.store = args[0];
        ref.id = args[1];
        remove(ref);
        std::cout << "removed credential [" << ref.store << "] " << ref.id << '\n';
        return commandSuccess();
    }

    std::cerr << "unknown credential command: " << head << " (try: help)\n";
    return commandFailure();
}

std::vector<std::string> CredentialManager::complete(const std::vector<std::string>& args,
                                                     std::size_t index) const {
    return filterByPrefix(kCredCommands, currentWord(args, index));
}

int DecoratedCredentialManager::invoke(std::vector<std::string>& args) {
    return m_wrapped->invoke(args);
}

std::vector<std::string> DecoratedCredentialManager::complete(
    const std::vector<std::string>& args, std::size_t index) const {
    return m_wrapped->complete(args, index);
}

std::filesystem::path FileCredentialManager::credentialPath() const { return m_path; }

bool FileCredentialManager::canReloadCredentials() const { return true; }

void FileCredentialManager::reloadCredentials() { reload(); }

void FileCredentialManager::saveCredentials() { flush(); }

std::size_t FileCredentialManager::credentialPersistedCount() const { return m_loadedCount; }

int FileCredentialManager::invoke(std::vector<std::string>& args) {
    return CredentialManager::invoke(args);
}

std::vector<std::string> FileCredentialManager::complete(const std::vector<std::string>& args,
                                                         std::size_t index) const {
    return CredentialManager::complete(args, index);
}

} // namespace bas::security
