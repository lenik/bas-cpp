#ifndef PROPERTY_SUPPORT_H
#define PROPERTY_SUPPORT_H

#include <boost/signals2.hpp>

#include <mutex>
#include <utility>

/**
 * Lightweight property with optional change notification.
 *
 * Example:
 *   mutable_property<int> count;
 *   count.onchange.add([](int new_val, int old_val) { log("count: %d -> %d", old_val, new_val); });
 *   count.set(42);
 *   int n = count.get();
 */
template <typename T> //
class observable {
  public:
    using value_type = T;

    using change_signal_t = boost::signals2::signal< //
        void(T const& new_value, T const& old_value)>;
    using slot_type = typename change_signal_t::slot_type;

    observable() //
        : onchange(std::make_unique<change_signal_t>()) {}

    explicit observable(T value)
        : m_value(std::move(value)), onchange(std::make_unique<change_signal_t>()) {}

    observable(observable&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.m_mutex);
        m_value = std::move(other.m_value);
        onchange = std::move(other.onchange);
    }

    // Move assignment
    observable& operator=(observable&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(m_mutex);
            std::lock_guard<std::mutex> lock2(other.m_mutex);
            m_value = std::move(other.m_value);
            onchange = std::move(other.onchange);
        }
        return *this;
    }

    // // Note: Returning T const& from a thread-safe getter is risky
    // // if the object is modified immediately after. Returning by value is safer.
    // T get() const {
    //     std::lock_guard<std::mutex> lock(m_mutex);
    //     return m_value;
    // }
    T& get() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }

    const T& get() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_value;
    }

    void set(T value) {
        T old;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_value != value) {
                old = std::move(m_value);
                m_value = std::move(value);
                changed = true;
            }
        }
        if (changed && onchange) {
            (*onchange)(m_value, old);
        }
    }

    void set_always(T value) {
        T old;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            old = std::move(m_value);
            m_value = std::move(value);
        }
        if (onchange) {
            (*onchange)(m_value, old);
        }
    }

    // Returns a pointer to the value
    T const* operator->() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return &m_value;
    }

    // DANGEROUS: Returns a mutable pointer
    T* operator->() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return &m_value;
    }

    boost::signals2::connection connect(const slot_type& slot) { //
        return onchange->connect(slot);
    }

    void disconnect() { //
        onchange->disconnect_all_slots();
    }

  private:
    mutable std::mutex m_mutex; // 'mutable' allows locking in const get()
    T m_value{};
    std::unique_ptr<change_signal_t> onchange;
};

#endif // PROPERTY_SUPPORT_H
