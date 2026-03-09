#ifndef URI_H
#define URI_H

#include <optional>
#include <string>

class URL;

class URI {
public:
    URI() = default;
    explicit URI(std::string spec);

    /** Full URI string (rebuilt from components). */
    std::string spec() const;
    bool empty() const;

    URI toURI() const { return *this; }
    URL toURL() const;

    /** Component getters: nullopt = not defined, "" = defined but empty. */
    const std::optional<std::string>& scheme() const { return m_scheme; }
    const std::optional<std::string>& authority() const { return m_authority; }
    const std::optional<std::string>& user() const { return m_user; }
    const std::optional<std::string>& password() const { return m_password; }
    const std::optional<std::string>& host() const { return m_host; }
    const std::optional<int>& port() const { return m_port; }
    const std::optional<std::string>& path() const { return m_path; }
    const std::optional<std::string>& dir() const { return m_dir; }
    const std::optional<std::string>& base() const { return m_base; }
    const std::optional<std::string>& query() const { return m_query; }
    const std::optional<std::string>& fragment() const { return m_fragment; }

    /** Build http URI: host, port (0 = default 80), path, optional query, optional fragment. */
    static URI http(const std::string& host, int port, const std::string& path,
                   const std::string& query = {}, const std::string& fragment = {});

    /** Build https URI. */
    static URI https(const std::string& host, int port, const std::string& path,
                    const std::string& query = {}, const std::string& fragment = {});

    /** Build ftp URI. */
    static URI ftp(const std::string& host, int port, const std::string& path,
                   const std::string& query = {}, const std::string& fragment = {});

    /** Build file URI from local path. */
    static URI file(const std::string& path);

protected:
    void parse(std::string_view s);
    void deriveDirBase();

    std::optional<std::string> m_scheme;
    std::optional<std::string> m_authority;
    std::optional<std::string> m_user;
    std::optional<std::string> m_password;
    std::optional<std::string> m_host;
    std::optional<int> m_port;
    std::optional<std::string> m_path;
    std::optional<std::string> m_dir;
    std::optional<std::string> m_base;
    std::optional<std::string> m_query;
    std::optional<std::string> m_fragment;
};

#endif // URI_H
