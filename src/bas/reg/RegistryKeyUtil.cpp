#include "RegistryKeyUtil.hpp"

#include <algorithm>
#include <set>

namespace os::reg {

namespace {

size_t minPos(size_t a, size_t b) {
    if (a == std::string::npos)
        return b;
    if (b == std::string::npos)
        return a;
    return std::min(a, b);
}

std::string firstSegment(std::string_view s) {
    size_t slash = std::string::npos;
    size_t dot = std::string::npos;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '/') {
            slash = i;
            break;
        }
        if (s[i] == '.') {
            dot = i;
            break;
        }
    }
    size_t end = minPos(slash, dot);
    if (end == std::string::npos)
        return std::string(s);
    return std::string(s.substr(0, end));
}

} // namespace

std::vector<std::string> listChildKeys(const std::map<std::string, std::string>& data,
                                       const std::string& node_key, bool full_key) {
    std::set<std::string> ordered;

    for (const auto& kv : data) {
        const std::string& key = kv.first;
        std::string seg;
        std::string fullOut;

        if (node_key.empty()) {
            seg = firstSegment(key);
            if (seg.empty())
                continue;
            fullOut = seg;
        } else {
            if (key == node_key)
                continue;
            const std::string slashPref = node_key + "/";
            const std::string dotPref = node_key + ".";
            if (key.rfind(slashPref, 0) == 0) {
                std::string rest = key.substr(slashPref.size());
                if (rest.empty())
                    continue;
                size_t slash = rest.find('/');
                size_t dot = rest.find('.');
                size_t end = minPos(slash, dot);
                if (end == std::string::npos)
                    end = rest.size();
                seg = rest.substr(0, end);
                if (seg.empty())
                    continue;
                fullOut = node_key + "/" + seg;
            } else if (key.rfind(dotPref, 0) == 0) {
                std::string rest = key.substr(dotPref.size());
                if (rest.empty())
                    continue;
                size_t dot = rest.find('.');
                seg = dot == std::string::npos ? rest : rest.substr(0, dot);
                if (seg.empty())
                    continue;
                fullOut = node_key + "." + seg;
            } else {
                continue;
            }
        }

        ordered.insert(full_key ? fullOut : seg);
    }

    return std::vector<std::string>(ordered.begin(), ordered.end());
}

std::vector<std::string> keysForDelTree(const std::map<std::string, std::string>& data,
                                        const std::string& node_key) {
    std::vector<std::string> out;
    const std::string slash = node_key + "/";
    const std::string dot = node_key + ".";
    for (const auto& kv : data) {
        const std::string& k = kv.first;
        if (k == node_key || k.rfind(slash, 0) == 0 || k.rfind(dot, 0) == 0)
            out.push_back(k);
    }
    return out;
}

} // namespace os::reg
