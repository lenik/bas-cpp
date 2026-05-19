#ifndef BAS_REG_SUB_HPP
#define BAS_REG_SUB_HPP

#include <boost/json.hpp>

#include <optional>

namespace bas::reg {

// RRL - Registry container locator

struct RRL {
    char separator{'/'};
    std::string container; // can be empty if not specified.
    std::string fragment;

    /**
     * @param separator the separator to use between the container and the fragment
     * @param container the container path, can contain the separator
     * @param fragment the internal path to use, must not contain the separator
     */
    RRL(char separator, std::string_view container, std::string_view fragment);
    RRL(const RRL& other);
    RRL(RRL&& other) noexcept;

    ~RRL() = default;

    static RRL _parse(std::string_view rrl, char separator = '/');

    RRL& operator=(const RRL& other);
    RRL& operator=(RRL&& other) noexcept;

    bool operator==(const RRL& other) const;
    bool operator!=(const RRL& other) const;
    bool operator<(const RRL& other) const;
    bool operator>(const RRL& other) const;
    bool operator<=(const RRL& other) const;
    bool operator>=(const RRL& other) const;

    std::string str() const;
};

class IContainerManager {
  public:
    /**
     * @return the container value, or std::nullopt if not found.
     */
    virtual std::optional<boost::json::value> readContainer(std::string_view container) const = 0;

    virtual void writeContainer(std::string_view container, const boost::json::value& value) = 0;

    /**
     * @return the cached container value, or nullptr if not found.
     */
    boost::json::value* cacheLoadContainer(std::string_view container);
    const boost::json::value* cacheLoadContainerForQuery(std::string_view container) const {
        return const_cast<IContainerManager*>(this)->cacheLoadContainer(container);
    }
    void cacheSaveContainer(std::string_view container, const boost::json::value& value);
    bool cacheRemoveContainer(std::string_view container);

    const boost::json::value* cacheLoadFragment(const RRL& rrl) const;
    bool cacheSaveFragment(const RRL& rrl, const boost::json::value& value);
    bool cacheRemoveFragment(const RRL& rrl);

    /** Write all in-memory containers marked dirty since last save. */
    void flushCachedContainers();

  protected:
    mutable std::unordered_map<std::string, boost::json::value> m_containerCache;
    mutable std::unordered_map<std::string, bool> m_containerDirty;
};

} // namespace bas::reg

#endif