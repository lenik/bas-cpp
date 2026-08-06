#include "Permission.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace bas::security {

std::vector<std::string_view> splitPermissionTokens(std::string_view permission) {
    std::vector<std::string_view> tokens;
    if (permission.empty()) {
        return tokens;
    }
    std::size_t start = 0;
    while (start < permission.size()) {
        const auto dot = permission.find('.', start);
        if (dot == std::string_view::npos) {
            tokens.push_back(permission.substr(start));
            break;
        }
        tokens.push_back(permission.substr(start, dot - start));
        start = dot + 1;
    }
    return tokens;
}

namespace {

bool needsQuotes(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    for (char ch : value) {
        if (ch == ';' || ch == '=' || ch == '"' || ch == '\'' || std::isspace(static_cast<unsigned char>(ch))) {
            return true;
        }
    }
    return false;
}

std::string quoteValue(std::string_view value) {
    if (!needsQuotes(value)) {
        return std::string(value);
    }
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char ch : value) {
        if (ch == '"' || ch == '\\') {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

/** Parse a possibly quoted value; advances @a i past the value. */
std::string parseValue(std::string_view text, std::size_t& i) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
        ++i;
    }
    if (i >= text.size()) {
        return {};
    }
    const char quote = text[i];
    if (quote == '"' || quote == '\'') {
        ++i;
        std::string value;
        while (i < text.size()) {
            if (text[i] == '\\' && i + 1 < text.size()) {
                value.push_back(text[i + 1]);
                i += 2;
                continue;
            }
            if (text[i] == quote) {
                ++i;
                break;
            }
            value.push_back(text[i]);
            ++i;
        }
        return value;
    }
    const std::size_t start = i;
    while (i < text.size() && text[i] != ';') {
        ++i;
    }
    std::size_t end = i;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

bool matchTokens(const std::vector<std::string_view>& pattern, std::size_t pi,
                 const std::vector<std::string_view>& permission, std::size_t gi) {
    if (pi == pattern.size()) {
        return gi == permission.size();
    }
    if (pattern[pi] == "**") {
        for (std::size_t k = gi; k <= permission.size(); ++k) {
            if (matchTokens(pattern, pi + 1, permission, k)) {
                return true;
            }
        }
        return false;
    }
    if (gi >= permission.size()) {
        return false;
    }
    if (pattern[pi] == "*" || pattern[pi] == permission[gi]) {
        return matchTokens(pattern, pi + 1, permission, gi + 1);
    }
    return false;
}

bool actionMatches(std::string_view patternAction, std::string_view permissionAction) {
    // Empty = all.
    if (patternAction.empty() || permissionAction.empty()) {
        return true;
    }
    const auto pat = splitPermissionTokens(patternAction);
    const auto perm = splitPermissionTokens(permissionAction);
    return matchTokens(pat, 0, perm, 0);
}

bool resourceMatches(std::string_view patternResource, std::string_view permissionResource) {
    // Empty = all; '*' also means all.
    if (patternResource.empty() || permissionResource.empty()) {
        return true;
    }
    if (patternResource == "*" || permissionResource == "*") {
        return true;
    }
    // Suffix / extension style: *.pdf
    if (patternResource.size() >= 2 && patternResource[0] == '*' && patternResource[1] == '.') {
        const auto suffix = patternResource.substr(1);
        return permissionResource.size() >= suffix.size() &&
               permissionResource.substr(permissionResource.size() - suffix.size()) == suffix;
    }
    // Path-style tokens with * / ** (slash-separated).
    if (patternResource.find('/') != std::string_view::npos ||
        patternResource.find("**") != std::string_view::npos ||
        patternResource.find('*') != std::string_view::npos) {
        auto splitPath = [](std::string_view text) {
            std::vector<std::string_view> out;
            if (text.empty())
                return out;
            if (!text.empty() && text.front() == '/')
                text.remove_prefix(1);
            while (!text.empty() && text.back() == '/')
                text.remove_suffix(1);
            std::size_t start = 0;
            for (std::size_t i = 0; i <= text.size(); ++i) {
                if (i == text.size() || text[i] == '/') {
                    out.emplace_back(text.data() + start, i - start);
                    start = i + 1;
                }
            }
            return out;
        };
        return matchTokens(splitPath(patternResource), 0, splitPath(permissionResource), 0);
    }
    return patternResource == permissionResource;
}

} // namespace

std::string Permission::toString() const {
    if (empty()) {
        return {};
    }
    std::string out;
    if (!action.empty()) {
        out += "action=";
        out += quoteValue(action);
    }
    if (!resource.empty()) {
        if (!out.empty()) {
            out.push_back(';');
        }
        out += "resource=";
        out += quoteValue(resource);
    }
    return out;
}

Permission Permission::parse(std::string_view text) {
    Permission permission;
    // Trim
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return permission;
    }

    const bool looksKeyed = text.find('=') != std::string_view::npos;
    if (!looksKeyed) {
        // Legacy shorthand: "action" or "action:resource" (unquoted).
        const auto colon = text.find(':');
        if (colon != std::string_view::npos && colon > 0) {
            permission.action = std::string(text.substr(0, colon));
            permission.resource = std::string(text.substr(colon + 1));
        } else {
            permission.action = std::string(text);
        }
        return permission;
    }

    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && (text[i] == ';' || std::isspace(static_cast<unsigned char>(text[i])))) {
            ++i;
        }
        if (i >= text.size()) {
            break;
        }
        const std::size_t keyStart = i;
        while (i < text.size() && text[i] != '=' && text[i] != ';') {
            ++i;
        }
        if (i >= text.size() || text[i] != '=') {
            // Malformed segment; skip to next ';'.
            while (i < text.size() && text[i] != ';') {
                ++i;
            }
            continue;
        }
        std::string_view key = text.substr(keyStart, i - keyStart);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.remove_suffix(1);
        }
        ++i; // skip '='
        const std::string value = parseValue(text, i);
        if (key == "action") {
            permission.action = value;
        } else if (key == "resource") {
            permission.resource = value;
        }
    }
    return permission;
}

bool DefaultPermissionMatcher::matches(const Permission& pattern,
                                       const Permission& permission) const {
    return actionMatches(pattern.action, permission.action) &&
           resourceMatches(pattern.resource, permission.resource);
}

int DefaultPermissionMatcher::specificity(const Permission& pattern) const {
    int score = 0;
    if (!pattern.resourceIsAll() && pattern.resource != "*") {
        score += 20;
    }
    if (pattern.actionIsAll()) {
        return score;
    }
    for (const auto token : splitPermissionTokens(pattern.action)) {
        if (token == "**") {
            score += 0;
        } else if (token == "*") {
            score += 1;
        } else {
            score += 10;
        }
    }
    return score;
}

} // namespace bas::security
