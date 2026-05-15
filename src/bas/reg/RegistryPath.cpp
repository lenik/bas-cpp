#include "RegistryPath.hpp"

namespace os::reg {

std::vector<std::string> splitKeyByChar(const std::string& s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start < s.size()) {
        size_t p = s.find(delim, start);
        if (p == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return out;
}

static std::string joinSlash(const std::vector<std::string>& parts) {
    std::string s;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i)
            s += '/';
        s += parts[i];
    }
    return s;
}

static std::string joinDot(const std::vector<std::string>& parts) {
    std::string s;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i)
            s += '.';
        s += parts[i];
    }
    return s;
}

std::string DualPathResolution::canonicalKey() const {
    return makeDualKey(dirSegments, fileStem, objectPath);
}

std::string makeDualKey(const std::vector<std::string>& dirSegments, const std::string& fileStem,
                        const std::vector<std::string>& objectPath) {
    std::string k = joinSlash(dirSegments);
    if (!k.empty())
        k += '/';
    k += fileStem;
    for (const auto& o : objectPath) {
        k += '.';
        k += o;
    }
    return k;
}

bool resolveDualPath(const std::string& key, DualPathResolution& out) {
    out = {};
    if (key.empty())
        return false;
    auto slashParts = splitKeyByChar(key, '/');
    if (slashParts.empty())
        return false;
    const std::string& last = slashParts.back();
    out.dirSegments.assign(slashParts.begin(), slashParts.end() - 1);
    auto dotParts = splitKeyByChar(last, '.');
    if (dotParts.empty())
        return false;
    out.fileStem = dotParts.front();
    out.objectPath.assign(dotParts.begin() + 1, dotParts.end());
    out.valid = true;
    return true;
}

std::vector<std::string> splitUnifiedRegistryPath(const std::string& key) {
    std::vector<std::string> out;
    std::string cur;
    cur.reserve(key.size());
    for (unsigned char uch : key) {
        char c = static_cast<char>(uch);
        if (c == '/' || c == '.') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else
            cur += c;
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

std::string canonicalUnifiedRegistryKey(const std::string& key) {
    return joinSlash(splitUnifiedRegistryPath(key));
}

bool resolveJsonTreePath(const std::string& key, JsonTreePathResolution& out) {
    out = {};
    if (key.empty())
        return false;
    auto slashParts = splitKeyByChar(key, '/');
    if (slashParts.empty())
        return false;

    const std::string& last = slashParts.back();
    const bool lastHasDot = last.find('.') != std::string::npos;

    if (slashParts.size() == 1) {
        if (!lastHasDot) {
            out.jsonFileRel = last + ".json";
            out.objectPath.clear();
            out.valid = true;
            return true;
        }
        auto dots = splitKeyByChar(last, '.');
        if (dots.empty())
            return false;
        out.jsonFileRel = dots[0] + ".json";
        out.objectPath.assign(dots.begin() + 1, dots.end());
        out.valid = true;
        return true;
    }

    if (!lastHasDot) {
        out.jsonFileRel = joinSlash(slashParts) + ".json";
        out.objectPath.clear();
        out.valid = true;
        return true;
    }

    std::vector<std::string> dirParts(slashParts.begin(), slashParts.end() - 1);
    out.jsonFileRel = joinSlash(dirParts) + ".json";
    out.objectPath = splitKeyByChar(last, '.');
    if (out.objectPath.empty())
        return false;
    out.valid = true;
    return true;
}

std::string makeJsonTreeKey(const std::string& jsonFileRel, const std::vector<std::string>& objectPath) {
    if (jsonFileRel.size() < 5 || jsonFileRel.compare(jsonFileRel.size() - 5, 5, ".json") != 0)
        return {};
    std::string base = jsonFileRel.substr(0, jsonFileRel.size() - 5);
    std::string obj = joinDot(objectPath);
    if (base.empty())
        return obj;
    return base + '/' + obj;
}

bool resolveFsPath(const std::string& key, FsPathResolution& out) {
    out = {};
    if (key.empty())
        return false;
    std::string flat;
    flat.reserve(key.size());
    for (char c : key) {
        if (c == '/' || c == '.')
            flat += '.';
        else
            flat += c;
    }
    auto segs = splitKeyByChar(flat, '.');
    if (segs.size() < 2)
        return false;
    size_t n = segs.size();
    out.leafKey = segs[n - 1];
    out.fileStem = segs[n - 2];
    out.dirSegments.reserve(n - 2);
    for (size_t i = 0; i + 2 < n; ++i)
        out.dirSegments.push_back(segs[i]);
    out.valid = true;
    return true;
}

std::string makeFsKey(const std::vector<std::string>& dirSegments, const std::string& fileStem,
                      const std::string& leafKey) {
    std::string k = joinSlash(dirSegments);
    if (!k.empty())
        k += '/';
    k += fileStem;
    k += '.';
    k += leafKey;
    return k;
}

std::string migrateLegacyFlatKey(const std::string& key) {
    if (key.find('/') != std::string::npos)
        return key;
    return joinSlash(splitKeyByChar(key, '.'));
}

std::string normalizeDualLookupKey(const std::string& key) {
    if (key.find('/') != std::string::npos)
        return key;
    return migrateLegacyFlatKey(key);
}

} // namespace os::reg
