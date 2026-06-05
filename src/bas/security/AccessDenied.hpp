#ifndef BAS_SECURITY_ACCESS_DENIED_HPP
#define BAS_SECURITY_ACCESS_DENIED_HPP

#include "Permission.hpp"

#include <stdexcept>
#include <string>

namespace bas::security {

class AccessDenied : public std::runtime_error {
  public:
    explicit AccessDenied(const Permission& permission)
        : std::runtime_error("access denied: " + permission.toString()), m_permission(permission) {}

    const Permission& permission() const { return m_permission; }

  private:
    Permission m_permission;
};

} // namespace bas::security

#endif
