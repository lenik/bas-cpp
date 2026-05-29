#include "command_support.hpp"

#include <algorithm>

namespace bas::security {

std::string shiftArg(std::vector<std::string>& args) {
    if (args.empty()) {
        return {};
    }
    std::string head = std::move(args.front());
    args.erase(args.begin());
    return head;
}

std::string takeFlag(std::vector<std::string>& args, const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == flag) {
            std::string value = args[i + 1];
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                        args.begin() + static_cast<std::ptrdiff_t>(i + 2));
            return value;
        }
    }
    return {};
}

bool hasFlag(const std::vector<std::string>& args, const std::string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

int commandSuccess() { return 0; }

int commandFailure() { return 1; }

std::string currentWord(const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        return {};
    }
    return args[index];
}

std::vector<std::string> filterByPrefix(const std::vector<std::string>& options,
                                          const std::string& prefix) {
    std::vector<std::string> matches;
    for (const auto& option : options) {
        if (prefix.empty() || option.compare(0, prefix.size(), prefix) == 0) {
            matches.push_back(option);
        }
    }
    return matches;
}

bool isKnownRealmType(std::string_view s) {
    return s == "global" || s == "device" || s == "app";
}

std::optional<Realm> parseAtRealmToken(const std::string& token) {
    if (token.empty() || token.front() != '@') {
        return std::nullopt;
    }
    Realm realm;
    const std::string_view rest(token.data() + 1, token.size() - 1);
    if (rest.empty()) {
        return std::nullopt;
    }
    const auto colon = rest.find(':');
    if (colon != std::string_view::npos) {
        realm.type = std::string(rest.substr(0, colon));
        realm.name = std::string(rest.substr(colon + 1));
    } else if (isKnownRealmType(rest)) {
        realm.type = std::string(rest);
    } else {
        realm.name = std::string(rest);
    }
    return realm;
}

Realm mergeRealm(Realm base, const Realm& overlay) {
    if (!overlay.type.empty()) {
        base.type = overlay.type;
    }
    if (!overlay.name.empty()) {
        base.name = overlay.name;
    }
    if (!overlay.uuid.empty()) {
        base.uuid = overlay.uuid;
    }
    if (!overlay.description.empty()) {
        base.description = overlay.description;
    }
    if (!overlay.image.empty()) {
        base.image = overlay.image;
    }
    return base;
}

void writeRealmSuffix(std::ostream& out, const Realm& realm) {
    if (realm.empty()) {
        out << " realm=(none)";
        return;
    }
    out << " realm=" << realmDisplayLabel(realm);
    if (!realm.uuid.empty()) {
        out << " uuid=" << realm.uuid;
    }
}

} // namespace bas::security
