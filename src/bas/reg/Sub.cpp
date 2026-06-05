#include "Sub.hpp"

#include "../script/json.hpp"

namespace bas::reg {

RRL::RRL(char separator, std::string_view container, std::string_view fragment)
    : separator(separator), container(container), fragment(fragment) {}

RRL::RRL(const RRL& other)
    : separator(other.separator), container(other.container), fragment(other.fragment) {}

RRL::RRL(RRL&& other) noexcept
    : separator(std::move(other.separator)), container(std::move(other.container)),
      fragment(std::move(other.fragment)) {}

RRL& RRL::operator=(const RRL& other) {
    separator = other.separator;
    container = other.container;
    fragment = other.fragment;
    return *this;
}

RRL& RRL::operator=(RRL&& other) noexcept {
    separator = std::move(other.separator);
    container = std::move(other.container);
    fragment = std::move(other.fragment);
    return *this;
}

bool RRL::operator==(const RRL& other) const {
    return separator == other.separator && container == other.container &&
           fragment == other.fragment;
}

bool RRL::operator!=(const RRL& other) const { return !(*this == other); }

bool RRL::operator<(const RRL& other) const {
    return separator < other.separator || container < other.container || fragment < other.fragment;
}

bool RRL::operator>(const RRL& other) const { return other < *this; }

bool RRL::operator<=(const RRL& other) const {
    return separator <= other.separator && container <= other.container &&
           fragment <= other.fragment;
}

bool RRL::operator>=(const RRL& other) const { return other <= *this; }

std::string RRL::str() const {
    if (container.empty()) {
        return fragment;
    }
    return container + separator + fragment;
}

std::vector<RRL> RRL::parse(std::string_view rrl, char separator) {
    if (!rrl.empty() && rrl.back() == separator)
        rrl = rrl.substr(0, rrl.size() - 1);
    if (rrl.empty())
        return {RRL(separator, "", "")};

    const size_t lastSlash = rrl.find_last_of(separator);
    std::string dir;
    std::string base;
    if (lastSlash == std::string::npos) {
        dir = "";
        base = rrl;
    } else {
        dir = rrl.substr(0, lastSlash);
        base = rrl.substr(lastSlash + 1);
    }

    std::vector<RRL> rrls;
    rrls.emplace_back(separator, dir, base);

    // bool dot_key = base.find('.') != std::string::npos;
    // if (!dot_key) // no dot in key, so it's a simple key
    rrls.emplace_back(separator, rrl, "");

    return rrls;
}

boost::json::value* IContainerManager::cacheLoadContainer(std::string_view _container) {
    std::string container = std::string(_container);
    auto cacheIt = m_containerCache.find(container);
    if (cacheIt != m_containerCache.end()) {
        return &cacheIt->second;
    }

    auto value = readContainer(container);
    m_containerDirty[container] = false;
    if (value.has_value()) {
        return &(m_containerCache[container] = *value);
    } else {
        m_containerCache.erase(container);
        return nullptr;
    }
}

void IContainerManager::cacheSaveContainer(std::string_view _container,
                                           const boost::json::value& value) {
    std::string container = std::string(_container);
    writeContainer(container, value);
    m_containerCache[container] = value;
    m_containerDirty[container] = false;
}

bool IContainerManager::cacheRemoveContainer(std::string_view _container) {
    std::string container = std::string(_container);
    auto n = m_containerCache.erase(container);
    m_containerDirty.erase(container);
    return n > 0;
}

const boost::json::value* IContainerManager::cacheLoadFragment(const RRL& rrl) const {
    const auto* json = cacheLoadContainer(rrl.container);
    if (!json)
        return nullptr;
    if (rrl.fragment.empty())
        return json;
    if (json->is_object() || json->is_array()) {
        auto loc = json::locate_const(*json, rrl.fragment);
        if (loc.node == nullptr)
            return nullptr;
        return loc.node;
    } else {
        return nullptr;
    }
}

bool IContainerManager::cacheSaveFragment(const RRL& rrl, const boost::json::value& value) {
    if (rrl.fragment.empty()) {
        cacheSaveContainer(rrl.container, value);
        return true;
    }

    boost::json::value* json = cacheLoadContainer(rrl.container);
    if (!json) {
        cacheSaveContainer(rrl.container, boost::json::object{});
        json = cacheLoadContainer(rrl.container);
        if (!json)
            return false;
    }
    if (!json->is_object()) {
        cacheSaveContainer(rrl.container, boost::json::object{});
        json = cacheLoadContainer(rrl.container);
        if (!json || !json->is_object())
            return false;
    }

    auto target = json::locate(*json, rrl.fragment, '/', true);
    if (target.node == nullptr) {
        return false;
    }
    *target.node = value;
    m_containerDirty[rrl.container] = true;
    return true;
}

bool IContainerManager::cacheRemoveFragment(const RRL& rrl) {
    if (rrl.fragment.empty()) {
        return cacheRemoveContainer(rrl.container);
    }

    boost::json::value* json = const_cast<boost::json::value*>(cacheLoadContainer(rrl.container));
    if (!json)
        return false;
    if (!json->is_object())
        return false;

    auto target = json::locate(*json, rrl.fragment);
    if (target.is_null())
        return false;

    if (target.is_object_field()) {
        auto& obj = target.parent->as_object();
        obj.erase(target.key);
        m_containerDirty[rrl.container] = true;
        return true;
    }

    if (target.is_array_item()) {
        auto& arr = target.parent->as_array();
        arr.erase(arr.begin() + target.index);
        m_containerDirty[rrl.container] = true;
        return true;
    }

    return false;
}

void IContainerManager::cacheClear() {
    m_containerCache.clear();
    m_containerDirty.clear();
}

void IContainerManager::flushCachedContainers() {
    for (const auto& [container, dirty] : m_containerDirty) {
        if (!dirty)
            continue;
        auto it = m_containerCache.find(container);
        if (it == m_containerCache.end())
            continue;
        writeContainer(container, it->second);
        m_containerDirty[container] = false;
    }
}

} // namespace bas::reg