#include "AbstractRegistry.hpp"

namespace os {

std::string AbstractRegistry::escapeJsonString(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string AbstractRegistry::parseJsonStringLiteral(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' || s.front() == '\r'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r'))
        s.remove_suffix(1);
    if (s.empty())
        return "";
    if (s.front() != '"')
        return std::string(s);

    std::string out;
    for (size_t i = 1; i < s.size(); ++i) {
        char c = s[i];
        if (c == '"')
            break;
        if (c == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case '\\':
                out += '\\';
                break;
            case '"':
                out += '"';
                break;
            default:
                out += s[i];
                break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

} // namespace os
