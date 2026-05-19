#ifndef BAS_REG_PATH_HPP
#define BAS_REG_PATH_HPP

#include <optional>
#include <string>
#include <vector>

namespace bas::reg {

/**
 * [ [dir/] base/] [domain.] key
 */
struct PathStruct {
    std::vector<std::string> dirSegments;
    std::optional<std::string> baseName;
    std::vector<std::string> domainSegments;
    std::string simpleKey;
    std::string str() const;

    PathStruct(const std::string& path);
    PathStruct(const PathStruct& other);
    PathStruct(PathStruct&& other) noexcept;

    ~PathStruct() = default;

    PathStruct& operator=(const PathStruct& other);
    PathStruct& operator=(PathStruct&& other) noexcept;

    bool operator==(const PathStruct& other) const;
    bool operator!=(const PathStruct& other) const;
    bool operator<(const PathStruct& other) const;
    bool operator>(const PathStruct& other) const;
    bool operator<=(const PathStruct& other) const;
    bool operator>=(const PathStruct& other) const;

  public:
    static PathStruct parse(const std::string& path);
};

} // namespace bas::reg

#endif // BAS_REG_PATH_HPP
