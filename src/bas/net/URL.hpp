#ifndef URL_H
#define URL_H

#include "URI.hpp"

#include <optional>
#include <string>

class URL {
public:
    URL() = default;
    explicit URL(std::string spec);

    /** Full URL string (rebuilt from components). */
    std::string spec() const { return m_uri.spec(); }
    bool empty() const { return m_uri.empty(); }

    /** Component getters: nullopt = not defined, "" = defined but empty. */
    const std::optional<std::string>& scheme() const { return m_uri.scheme(); }
    const std::optional<std::string>& authority() const { return m_uri.authority(); }
    const std::optional<std::string>& user() const { return m_uri.user(); }
    const std::optional<std::string>& password() const { return m_uri.password(); }
    const std::optional<std::string>& host() const { return m_uri.host(); }
    const std::optional<int>& port() const { return m_uri.port(); }
    const std::optional<std::string>& path() const { return m_uri.path(); }
    const std::optional<std::string>& dir() const { return m_uri.dir(); }
    const std::optional<std::string>& base() const { return m_uri.base(); }
    const std::optional<std::string>& query() const { return m_uri.query(); }
    const std::optional<std::string>& fragment() const { return m_uri.fragment(); }

    /** Scheme (e.g. "http", "file"). Empty string if not defined. */
    std::string getScheme() const { return m_uri.scheme().value_or(""); }

    /** Path component; for file: returns local path (e.g. C:/foo or /tmp/foo). */
    std::string getPath() const;

    URI toURI() const { return m_uri; }
    URL toURL() const { return *this; }

    /** Build http URL. port 0 = default 80. */
    static URL http(const std::string& host, int port, const std::string& path,
                   const std::string& query = {}, const std::string& fragment = {});

    /** Build https URL. port 0 = default 443. */
    static URL https(const std::string& host, int port, const std::string& path,
                    const std::string& query = {}, const std::string& fragment = {});

    /** Build ftp URL. */
    static URL ftp(const std::string& host, int port, const std::string& path,
                   const std::string& query = {}, const std::string& fragment = {});

    /** Build file URL from local path. */
    static URL local(const std::string& path);

    /** Build URL for an entry inside a zip file. Scheme zip: path "path/to.zip!/entry". */
    static URL zipEntry(const std::string& zipPath, const std::string& zipEntry);

private:
    URI m_uri;
};

#endif // URL_H
