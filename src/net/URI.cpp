#include "URI.hpp"

#include "URL.hpp"

#include <cctype>
#include <sstream>
#include <string_view>

namespace {

std::string_view trimScheme(std::string_view s, std::size_t& i) {
    // scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
    if (i >= s.size() || !std::isalpha(static_cast<unsigned char>(s[i]))) return {};
    std::size_t start = i;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isalnum(c) || c == '+' || c == '-' || c == '.') ++i;
        else break;
    }
    return s.substr(start, i - start);
}

void parseAuthority(std::string_view auth, std::optional<std::string>& user,
                   std::optional<std::string>& password, std::optional<std::string>& host,
                   std::optional<int>& port) {
    if (auth.empty()) return;
    std::size_t at = auth.rfind('@');
    std::string_view hostPort;
    if (at != std::string_view::npos) {
        std::string_view userInfo = auth.substr(0, at);
        hostPort = auth.substr(at + 1);
        std::size_t colon = userInfo.find(':');
        if (colon != std::string_view::npos) {
            user = std::string(userInfo.substr(0, colon));
            password = std::string(userInfo.substr(colon + 1));
        } else {
            user = std::string(userInfo);
        }
    } else {
        hostPort = auth;
    }
    if (hostPort.empty()) return;
    std::size_t colon = hostPort.rfind(':');
    if (colon != std::string_view::npos && colon > 0) {
        std::string_view portStr = hostPort.substr(colon + 1);
        bool allDigit = true;
        for (char c : portStr) { if (!std::isdigit(static_cast<unsigned char>(c))) { allDigit = false; break; } }
        if (allDigit && !portStr.empty()) {
            try {
                port = std::stoi(std::string(portStr));
            } catch (...) {}
            host = std::string(hostPort.substr(0, colon));
            return;
        }
    }
    host = std::string(hostPort);
}

} // namespace

void URI::parse(std::string_view s) {
    m_scheme = std::nullopt;
    m_authority = std::nullopt;
    m_user = m_password = m_host = std::nullopt;
    m_port = std::nullopt;
    m_path = m_dir = m_base = std::nullopt;
    m_query = m_fragment = std::nullopt;

    if (s.empty()) return;

    std::size_t i = 0;
    std::string_view sch = trimScheme(s, i);
    if (!sch.empty() && i < s.size() && s[i] == ':') {
        m_scheme = std::string(sch);
        ++i;
        if (i + 2 <= s.size() && s[i] == '/' && s[i + 1] == '/') {
            i += 2;
            std::size_t start = i;
            while (i < s.size() && s[i] != '/' && s[i] != '?' && s[i] != '#') ++i;
            if (i > start) {
                m_authority = std::string(s.substr(start, i - start));
                parseAuthority(s.substr(start, i - start), m_user, m_password, m_host, m_port);
            }
        }
    } else {
        i = 0;
    }

    std::size_t pathStart = i;
    while (i < s.size() && s[i] != '?' && s[i] != '#') ++i;
    if (i > pathStart) {
        m_path = std::string(s.substr(pathStart, i - pathStart));
        deriveDirBase();
    }

    if (i < s.size() && s[i] == '?') {
        ++i;
        std::size_t start = i;
        while (i < s.size() && s[i] != '#') ++i;
        m_query = std::string(s.substr(start, i - start));
    }
    if (i < s.size() && s[i] == '#') {
        ++i;
        m_fragment = std::string(s.substr(i));
    }
}

void URI::deriveDirBase() {
    if (!m_path) return;
    const std::string& p = *m_path;
    if (p.empty()) return;
    std::size_t last = p.rfind('/');
    if (last == std::string::npos) {
        m_dir = std::nullopt;
        m_base = p;
        return;
    }
    if (last == 0) {
        m_dir = "/";
        m_base = (last + 1 < p.size()) ? p.substr(last + 1) : std::string("");
        return;
    }
    m_dir = p.substr(0, last);
    m_base = (last + 1 < p.size()) ? p.substr(last + 1) : std::string("");
}

URI::URI(std::string spec) {
    if (spec.empty()) throw std::invalid_argument("URI::URI: spec is empty");
    parse(spec);
}

std::string URI::spec() const {
    std::ostringstream o;
    if (m_scheme) o << *m_scheme << ":";
    if (m_authority) o << "//" << *m_authority;
    if (m_path) o << *m_path;
    if (m_query) o << "?" << *m_query;
    if (m_fragment) o << "#" << *m_fragment;
    return o.str();
}

bool URI::empty() const {
    return !m_scheme && !m_authority && !m_path && !m_query && !m_fragment;
}

URL URI::toURL() const {
    return URL(spec());
}

static void appendPath(std::ostream& out, const std::string& path) {
    if (path.empty() || path[0] != '/') out << '/';
    out << path;
}

URI URI::http(const std::string& host, int port, const std::string& path,
             const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URI::http: host is empty");
    URI u;
    u.m_scheme = "http";
    u.m_host = host;
    if (port > 0 && port != 80) u.m_port = port;
    u.m_authority = host;
    if (u.m_port) u.m_authority = *u.m_authority + ":" + std::to_string(*u.m_port);
    u.m_path = path.empty() ? "/" : (path[0] == '/' ? path : "/" + path);
    u.deriveDirBase();
    if (!query.empty()) u.m_query = query;
    if (!fragment.empty()) u.m_fragment = fragment;
    return u;
}

URI URI::https(const std::string& host, int port, const std::string& path,
              const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URI::https: host is empty");
    URI u;
    u.m_scheme = "https";
    u.m_host = host;
    if (port > 0 && port != 443) u.m_port = port;
    u.m_authority = host;
    if (u.m_port) u.m_authority = *u.m_authority + ":" + std::to_string(*u.m_port);
    u.m_path = path.empty() ? "/" : (path[0] == '/' ? path : "/" + path);
    u.deriveDirBase();
    if (!query.empty()) u.m_query = query;
    if (!fragment.empty()) u.m_fragment = fragment;
    return u;
}

URI URI::ftp(const std::string& host, int port, const std::string& path,
            const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URI::ftp: host is empty");
    URI u;
    u.m_scheme = "ftp";
    u.m_host = host;
    if (port > 0 && port != 21) u.m_port = port;
    u.m_authority = host;
    if (u.m_port) u.m_authority = *u.m_authority + ":" + std::to_string(*u.m_port);
    u.m_path = path.empty() ? "/" : (path[0] == '/' ? path : "/" + path);
    u.deriveDirBase();
    if (!query.empty()) u.m_query = query;
    if (!fragment.empty()) u.m_fragment = fragment;
    return u;
}

URI URI::file(const std::string& path) {
    if (path.empty()) throw std::invalid_argument("URI::file: path is empty");
    URI u;
    u.m_scheme = "file";
    std::string p = path;
    if (p.empty()) {
        u.m_path = "";
        return u;
    }
    if (p.size() >= 3 && p[1] == ':' && (p[2] == '/' || p[2] == '\\'))
        p = "/" + p;
    for (char& c : p) if (c == '\\') c = '/';
    if (p[0] != '/') p = "/" + p;
    u.m_path = p;
    u.deriveDirBase();
    return u;
}
