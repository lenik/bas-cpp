#ifndef PATH_HPP
#define PATH_HPP

#include <string>
#include <string_view>

/**
 * Immutable UNIX-style path helper with cached components.
 *
 * Semantics:
 * - isAbsolute(): leading '/' only.
 * - Path(".../subdir/") => trailing slash kept, base() is empty.
 * - Components are cached; all mutating-style operations return a new Path.
 */
class Path {
public:
    Path(std::string_view path);
    Path(std::string_view dir, std::string_view base);
    Path(const Path&);
    Path(Path&&) noexcept;
    Path& operator=(const Path&);
    Path& operator=(Path&&) noexcept;

    const std::string& str() const { return m_path; }
    // bool empty() const { return m_path.empty(); }

    bool equals(const Path& other) const { return m_path == other.m_path; }
    bool notEquals(const Path& other) const { return m_path != other.m_path; }
    int compare(const Path& other) const;

    bool operator==(const Path& other) const { return equals(other); }
    bool operator!=(const Path& other) const { return notEquals(other); }
    bool operator<(const Path& other) const { return compare(other) < 0; }
    bool operator>(const Path& other) const { return compare(other) > 0; }
    bool operator<=(const Path& other) const { return compare(other) <= 0; }
    bool operator>=(const Path& other) const { return compare(other) >= 0; }

    // Parent directory (chops last segment; preserves leading slash when absolute).
    Path getParent() const;

    // Directory part, without trailing slash.
    std::string dir() const;
    
    // Directory part, with trailing slash if not empty.
    std::string dir_() const;

    // Last path segment (without trailing slash). Empty when path ends with '/' and has no non-empty segment.
    std::string base() const;

    // Name without extension.
    std::string name() const;

    // Extension of last segment. withDot=false: "txt"; withDot=true: ".txt".
    std::string extension(bool withDot = false) const;

    /** Return a new Path with the directory part set to \a newDir (base unchanged). */
    Path dir(std::string_view newDir) const;

    /** Return a new Path with the last segment set to \a newBase. */
    Path base(std::string_view newBase) const;

    /** Return a new Path with the name (no extension) set to \a newName. */
    Path name(std::string_view newName) const;

    /** Return a new Path with the extension set to \a newExtension (leading dot optional). */
    Path extension(std::string_view newExtension) const;

    bool isAbsolute() const { return !m_path.empty() && m_path[0] == '/'; }

    // Ensure leading slash.
    Path toAbsolute() const;
    Path prependSlash() const { return toAbsolute(); }

    // Remove leading slash if any.
    Path toRelative() const;
    Path shiftSlash() const { return toRelative(); }

    // Ensure trailing slash.
    Path toDir() const;
    Path appendSlash() const { return toDir(); }

    // Remove trailing slash (except when path is "/" ).
    Path toFile() const;
    Path chopSlash() const { return toFile(); }

    // Normalize path: collapse "." and ".." segments, remove redundant slashes.
    Path normalize() const;

    /**
     * Resolve href-style reference:
     * - If other.isAbsolute() -> other.normalize()
     * - Else -> (toDir().join(other)).normalize()
     */
    Path href(const Path& other) const;

    /**
     * Simple join:
     * - If other.isAbsolute() -> other
     * - Else -> Path(toDir().str() + other.str())
     */
    Path join(const Path& other) const;

private:
    void initCache();

    std::string m_path;

public:
    const char* _path;       // m_path.data()
    int _len;
    int _dir_len;           // parent length without trailing slash; -1 if none
    int _dir_slash_len;     // parent length with trailing slash; 0 if none
    const char* _base;      // start of last segment
    int _name_len;          // length of name (no extension)
    const char* _ext;       // extension without dot; null if none
    const char* _dotExt;    // points to '.' or to end of string if no extension
};

#include <unordered_map>

template<typename T>
class PathMap {
    std::unordered_map<std::string, T> m_map;

public:
    T get(const Path& path) const {
        return m_map.at(path.str());
    }

    void set(const Path& path, T value) {
        m_map[path.str()] = value;
    }

    void erase(const Path& path) {
        m_map.erase(path.str());
    }

    void clear() {
        m_map.clear();
    }

    T operator[](const Path& path) const {
        return m_map.at(path.str());
    }

    T& operator[](const Path& path) {
        return m_map.at(path.str());
    }
};

#endif // PATH_HPP