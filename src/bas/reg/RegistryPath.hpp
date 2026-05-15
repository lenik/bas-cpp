#ifndef OMNISHELL_CORE_REGISTRY_PATH_HPP
#define OMNISHELL_CORE_REGISTRY_PATH_HPP

#include <string>
#include <vector>

namespace os::reg {

std::vector<std::string> splitKeyByChar(const std::string& s, char delim);

/**
 * Dual layout: '/' = filesystem (dirs + JSON file stem), '.' = object path inside that file.
 * Example: foo/bar/cat.dog.name1 -> file root/foo/bar/cat.json, object dog.name1, leaf name1.
 */
struct DualPathResolution {
    bool valid{false};
    std::vector<std::string> dirSegments;
    std::string fileStem;
    std::vector<std::string> objectPath;

    std::string canonicalKey() const;
};

bool resolveDualPath(const std::string& key, DualPathResolution& out);

/**
 * Json-tree layout: '/' and '.' both map into JSON; the last '/' field names the object path;
 * the file is parent path + .json.
 * Example: foo/bar/cat.dog.name1 -> file root/foo/bar.json, object cat.dog.name1.
 */
struct JsonTreePathResolution {
    bool valid{false};
    /** Relative path with .json, e.g. foo/bar.json */
    std::string jsonFileRel;
    std::vector<std::string> objectPath;
};

bool resolveJsonTreePath(const std::string& key, JsonTreePathResolution& out);

/**
 * Unified layout for in-memory JsonRegistry only: '/' and '.' are interchangeable segment
 * separators. Canonical map keys join segments with '/'.
 */
std::vector<std::string> splitUnifiedRegistryPath(const std::string& key);
std::string canonicalUnifiedRegistryKey(const std::string& key);

/**
 * Fs layout: '/' and '.' are both path segments; second-to-last segment is JSON filename stem.
 * Example: foo/bar/cat.dog.name1 -> file root/foo/bar/cat/dog.json, { "name1": "..." }.
 */
struct FsPathResolution {
    bool valid{false};
    std::vector<std::string> dirSegments;
    std::string fileStem;
    std::string leafKey;
};

bool resolveFsPath(const std::string& key, FsPathResolution& out);

std::string makeDualKey(const std::vector<std::string>& dirSegments, const std::string& fileStem,
                        const std::vector<std::string>& objectPath);

std::string makeJsonTreeKey(const std::string& jsonFileRel, const std::vector<std::string>& objectPath);

std::string makeFsKey(const std::vector<std::string>& dirSegments, const std::string& fileStem,
                      const std::string& leafKey);

/**
 * Legacy API keys used only '.' (no '/'). Map to the old on-disk slash layout a/b/c
 * so existing data and dotted call sites keep working.
 */
std::string migrateLegacyFlatKey(const std::string& key);

/** If key contains '/', use as Dual key; else apply migrateLegacyFlatKey. */
std::string normalizeDualLookupKey(const std::string& key);

} // namespace os::reg

#endif
