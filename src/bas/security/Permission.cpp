#include "Permission.hpp"

#include <algorithm>

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

} // namespace

bool DefaultPermissionMatcher::matches(const Permission& pattern,
                                       const Permission& permission) const {
    if (!pattern.resource.empty() || !permission.resource.empty()) {
        if (pattern.resource != permission.resource && pattern.resource != "*" &&
            !pattern.resource.empty()) {
            return false;
        }
        if (pattern.resource != permission.resource && permission.resource != "*" &&
            !permission.resource.empty()) {
            return false;
        }
    }
    const std::string patternText = pattern.pattern();
    const std::string permissionText = permission.pattern();
    const auto pat = splitPermissionTokens(patternText);
    const auto perm = splitPermissionTokens(permissionText);
    return matchTokens(pat, 0, perm, 0);
}

int DefaultPermissionMatcher::specificity(const Permission& pattern) const {
    int score = 0;
    if (!pattern.resource.empty() && pattern.resource != "*") {
        score += 20;
    }
    const std::string patternText = pattern.pattern();
    for (const auto token : splitPermissionTokens(patternText)) {
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
