#include "Path.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

namespace {

/** Compare path strings field-by-field (segment by segment), C-style. No allocation. Trailing slash = directory sorts before same path as file. */
static int pathCompareC(const char* a, const char* b) {
    for (;;) {
        char ca = *a;
        if (ca) {
            char cb = *b;
            if (cb)  {
                if (ca != cb) {
                    if (ca == '/') return -1;
                    if (cb == '/') return 1;
                    return (unsigned char)ca < (unsigned char)cb ? -1 : 1;
                }
                ++a;
                ++b;
            } else {
                return 1;
            }
        } else {
            if (*b) {
                return -1;
            } else {
                return 0;
            }
        }
    }
    return 0;
}

} // namespace

int Path::compare(const Path& other) const {
    return pathCompareC(_path, other._path);
}

static std::string path_join(std::string_view dir, std::string_view base) {
    if (!base.empty() && base[0] == '/')
        return std::string(base);
    
    std::string path = std::string(dir);
    if (!dir.empty() && dir.back() != '/') {
        path.append("/");
    }
    path.append(base);
    return path;
}

Path::Path(std::string_view dir, std::string_view base)
    : Path(path_join(dir, base))
{
}

Path::Path(std::string_view path)
    : m_path(path)
{
    initCache();
}

Path::Path(const Path& other)
    : m_path(other.m_path)
{
    initCache();
}

Path::Path(Path&& other) noexcept
    : m_path(std::move(other.m_path))
{
    initCache();
}

Path& Path::operator=(const Path& other) {
    m_path = other.m_path;
    initCache();
    return *this;
}

Path& Path::operator=(Path&& other) noexcept {
    m_path = std::move(other.m_path);
    initCache();
    return *this;
}

void Path::initCache() {
    _path = m_path.data();
    _len = static_cast<int>(m_path.size());
    if (_len == 0) {
        _dir_len = -1;
        _dir_slash_len = 0;
        _base = _path;
        _name_len = 0;
        _ext = nullptr;
        _dotExt = _base;
        return;
    }
    int pos = static_cast<int>(m_path.find_last_of('/'));
    if (pos == -1) {
        _dir_len = -1;
        _dir_slash_len = 0;
        _base = _path;
    } else {
        _dir_len = static_cast<int>(pos);
        _dir_slash_len = static_cast<int>(pos + 1);
        _base = _path + pos + 1;
    }
    const int base_len = _len - _dir_slash_len;
    const char* base_end = _base + base_len;
    const char* dot = nullptr;
    for (int i = base_len - 1; i > 0; --i) {
        if (_base[i] == '.') { dot = _base + i; break; }
    }
    if (dot == nullptr || dot == _base) {
        _name_len = base_len;
        _ext = nullptr;
        _dotExt = base_end;
    } else {
        _name_len = static_cast<int>(dot - _base);
        _ext = dot + 1;
        _dotExt = dot;
    }
    assert(_name_len >= 0);
}

std::string Path::base() const {
    const int base_len = (m_path.size() > 1 && m_path.back() == '/') ? 0 : static_cast<int>(m_path.size() - _dir_slash_len);
    return std::string(_base, base_len);
}

std::string Path::name() const {
    try {
        return std::string(_base, _name_len);
    } catch (const std::exception& e) {
        printf("error get name(): _base=%s, _ext=%s, nl=%d\n", _base, _ext, _name_len);
        throw;
    }
}

std::string Path::extension(bool withDot) const {
    if (!_ext || *_dotExt != '.')
        return std::string();
    const int base_len = (m_path.size() > 1 && m_path.back() == '/') ? 0 : static_cast<int>(m_path.size() - _dir_slash_len);
    const size_t ext_len = static_cast<size_t>(base_len - _name_len - 1);
    if (withDot)
        return std::string(_dotExt, 1 + ext_len);
    return std::string(_ext, ext_len);
}

Path Path::dir(std::string_view newDir) const {
    std::string path(newDir);
    if (!newDir.empty() && newDir.back() != '/') {
        path.append("/");
    }
    path.append(_base);
    return Path(path);
}

Path Path::base(std::string_view newBase) const {
    std::string path(dir_());
    path.append(newBase);
    return Path(path);
}

Path Path::name(std::string_view newName) const {
    std::string path(dir_());
    path.append(newName);
    path.append(_dotExt);
    return Path(path);
}

Path Path::extension(std::string_view newExtension) const {
    std::string path(dir_());
    path.append(name());
    if (!newExtension.empty()) {
        if (newExtension[0] != '.')
            path.append(".");
        path.append(newExtension);
    }
    return Path(path);
}

Path Path::toAbsolute() const {
    if (isAbsolute())
        return *this;
    std::string p = m_path;
    if (p.empty() || p[0] != '/')
        p.insert(p.begin(), '/');
    return Path(p);
}

Path Path::toRelative() const {
    if (!isAbsolute())
        return *this;
    if (m_path.empty())
        return *this;
    if (m_path == "/")
        return *this;
    // drop single leading slash
    return Path(m_path.substr(1));
}

Path Path::normalize() const {
    int old_len = static_cast<int>(m_path.size());
    if (old_len == 0)
        return Path("");

    const char* old = _path;
    const char *end = old + old_len;

    char buf[old_len + 1];
    int len = 0;
    int parent = 0;
    
    while (end > old) {
        int slash = 0;
        int dots = 0;
        int al = 0;
        const char* token = end;
        while (token > old) {
            switch (*(--token)) {
                case '/': slash = 1; break;
                case '.': dots++; break;
                default: al++; break;
            }
            if (slash) break;
        }
        
        if (!al) {
            if (slash) {
                if (dots == 1) {
                    continue; // skip ./
                } else if (dots == 2) {
                    parent++;
                    continue;
                } else { // same as al+slash
                    al = 1;
                }
            } else {
                // . .. at the beginning, same as al
                al = 1;
            }
        }
        if (al) {
            if (slash) {
                if (parent) {
                    parent--;
                    continue;
                }else {
                    break; // concat
                }
            } else {
                if (parent) {
                    parent--;
                    buf[len++] = '.';
                    continue;
                } else {
                    break;
                }
            }
        }
        // concat reverse(p..end)
        while (end > token) {
            buf[len++] = *(--end);
        }
    }

    while (len >= 2 && buf[len - 2] == '/' && buf[len - 1] == '.')
        len -= 2;
    while (parent > 0) {
        --parent;
        buf[len++] = '/';
        buf[len++] = '.';
        buf[len++] = '.';
    }

    // reverse the buffer
    for (int i = 0; i < len / 2; ++i) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    buf[len] = '\0';

    if (len == old_len)
        return *this;
    return Path(std::string(buf, len));
}

Path Path::getParent() const {
    return _dir_len == -1 ? Path("") : Path(dir());
}

std::string Path::dir() const {
    if (_dir_len == -1) return std::string();
    return std::string(_path, _dir_len);
}

std::string Path::dir_() const {
    if (_dir_len == -1) return std::string();
    return std::string(_path, _dir_slash_len);
}

Path Path::toDir() const {
    if (m_path.empty())
        return Path("/");
    if (m_path.back() == '/')
        return *this;
    return Path(m_path + "/");
}

Path Path::toFile() const {
    if (m_path.size() > 1 && m_path.back() == '/') {
        return Path(m_path.substr(0, m_path.size() - 1));
    }
    return *this;
}

Path Path::join(const Path &other) const {
    if (other._path == nullptr) throw std::invalid_argument("Path::join: other _path is null");
    if (other.isAbsolute())
        return other;
    std::string path(_path);
    if (path.back() != '/')
        path.append("/");
    path.append(other._path);
    return Path(path);
}

Path Path::href(const Path &other) const {
    if (other._path == nullptr) throw std::invalid_argument("Path::href: other _path is null");
    if (other.isAbsolute())
        return other;
    std::string path(dir_());
    path.append(other._path);
    return Path(path);
}
