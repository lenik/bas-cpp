#include "URL.hpp"

#include <algorithm>
#include <stdexcept>

URL::URL(std::string spec)
    : m_uri(spec)
{
}

std::string URL::getPath() const {
    std::string p = m_uri.path().value_or("");
    if (m_uri.scheme() == "file" && p.size() >= 3 && p[0] == '/' && p[2] == ':')
        p = p.substr(1);  // /C:/ -> C:/
    return p;
}

URL URL::http(const std::string& host, int port, const std::string& path,
              const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URL::http: host is empty");
    return URL(URI::http(host, port, path, query, fragment).spec());
}

URL URL::https(const std::string& host, int port, const std::string& path,
              const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URL::https: host is empty");
    return URL(URI::https(host, port, path, query, fragment).spec());
}

URL URL::ftp(const std::string& host, int port, const std::string& path,
             const std::string& query, const std::string& fragment) {
    if (host.empty()) throw std::invalid_argument("URL::ftp: host is empty");
    return URL(URI::ftp(host, port, path, query, fragment).spec());
}

URL URL::local(const std::string& path) {
    if (path.empty()) throw std::invalid_argument("URL::local: path is empty");
    return URL(URI::file(path).spec());
}

URL URL::zipEntry(const std::string& zipPath, const std::string& zipEntryPath) {
    if (zipPath.empty()) throw std::invalid_argument("URL::zipEntry: zipPath is empty");
    if (zipEntryPath.empty()) throw std::invalid_argument("URL::zipEntry: zipEntryPath is empty");
    std::string z = zipPath;
    std::replace(z.begin(), z.end(), '\\', '/');
    std::string e = zipEntryPath;
    std::replace(e.begin(), e.end(), '\\', '/');
    if (!e.empty() && e[0] == '/') e = e.substr(1);
    return URL("zip:" + z + "!/" + e);
}
