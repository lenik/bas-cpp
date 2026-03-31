#ifndef EVENT_SUPPORT_H
#define EVENT_SUPPORT_H

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

/**
 * Lightweight event / multicast callback support.
 *
 * Example:
 *   event_support<void(const std::string&)> on_error;
 *   on_error.add([](const std::string& msg) { log(msg); });
 *   auto h = on_error.add(std::function<void(const std::string&)>(f));
 *   on_error.remove(h);   // remove by handle
 *   on_error.remove(f);   // remove by same std::function (same target)
 *   on_error.fire("failed");
 */
template<typename Signature>
class event_support;

template<typename R, typename... Args>
class event_support<R(Args...)> {
public:
    using callback_type = std::function<R(Args...)>;
    using handle_type = std::size_t;

    /** Add a listener; returns a handle for removal. */
    handle_type add(callback_type f) {
        handle_type id = m_next_id++;
        m_listeners.push_back({id, std::move(f)});
        return id;
    }

    /** Remove the listener with the given handle (returned from add). */
    void remove(handle_type h) {
        m_listeners.erase(
            std::remove_if(m_listeners.begin(), m_listeners.end(),
                [h](const auto& p) { return p.first == h; }),
            m_listeners.end());
    }

    /**
     * Remove the first listener that has the same target as f.
     * Works when f is the same std::function object (or a copy sharing the target) that was passed to add.
     */
    void remove(callback_type const& f) {
        using fn_t = R(Args...);
        auto* fp = f.template target<fn_t>();
        if (!fp) return;
        auto it = std::find_if(m_listeners.begin(), m_listeners.end(),
            [fp](const auto& p) {
                auto* tp = p.second.template target<fn_t>();
                return tp && tp == fp;
            });
        if (it != m_listeners.end())
            m_listeners.erase(it);
    }

    void clear() {
        m_listeners.clear();
        m_next_id = 0;
    }

    /** Invoke all listeners with the given arguments. */
    void fire(Args... args) const {
        for (const auto& p : m_listeners) {
            if (p.second)
                p.second(std::forward<Args>(args)...);
        }
    }

private:
    handle_type m_next_id{0};
    std::vector<std::pair<handle_type, callback_type>> m_listeners;
};

#endif // EVENT_SUPPORT_H
