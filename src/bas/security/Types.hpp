#ifndef BAS_SECURITY_TYPES_HPP
#define BAS_SECURITY_TYPES_HPP

#include <boost/json.hpp>

#include <string>
#include <string_view>

namespace bas::security {

using QualifiedToken = std::string;
using JsonObject = boost::json::object;

class IdentityState {
  public:
    enum class Value {
        UNKNOWN = 0,
        ACTIVE,
        EXPIRED,
        REVOKED,
        LOGGED_OUT,
    };

  private:
    Value m_val = Value::UNKNOWN;

  public:
    constexpr IdentityState() = default;
    constexpr IdentityState(Value v) : m_val(v) {}

    constexpr operator Value() const noexcept { return m_val; }
    constexpr Value value() const noexcept { return m_val; }

    constexpr bool isActive() const noexcept { return m_val == Value::ACTIVE; }

    static IdentityState fromString(std::string_view s);
    std::string str() const;

    constexpr bool operator==(IdentityState other) const noexcept { return m_val == other.m_val; }
    constexpr bool operator!=(IdentityState other) const noexcept { return m_val != other.m_val; }

    static const IdentityState Unknown;
    static const IdentityState Active;
    static const IdentityState Expired;
    static const IdentityState Revoked;
    static const IdentityState LoggedOut;
};

class IdentitySource {
  public:
    enum class Value {
        UNKNOWN = 0,
        DIRECT,
        DERIVED,
        LOGIN,
        AUTO,
        SYSTEM,
    };

  private:
    Value m_val = Value::UNKNOWN;

  public:
    constexpr IdentitySource() = default;
    constexpr IdentitySource(Value v) : m_val(v) {}

    constexpr operator Value() const noexcept { return m_val; }
    constexpr Value value() const noexcept { return m_val; }

    int priority() const noexcept;

    static IdentitySource fromString(std::string_view s);
    std::string str() const;

    constexpr bool operator==(IdentitySource other) const noexcept { return m_val == other.m_val; }
    constexpr bool operator!=(IdentitySource other) const noexcept { return m_val != other.m_val; }

    static const IdentitySource Unknown;
    static const IdentitySource Direct;
    static const IdentitySource Derived;
    static const IdentitySource Login;
    static const IdentitySource Auto;
    static const IdentitySource System;
};

class LoginStatus {
  public:
    enum class Value {
        UNKNOWN = 0,
        SUCCESS,
        NEED_CREDENTIAL,
        NEED_INTERACTION,
        FAILED,
        UNSUPPORTED,
    };

  private:
    Value m_val = Value::UNKNOWN;

  public:
    constexpr LoginStatus() = default;
    constexpr LoginStatus(Value v) : m_val(v) {}

    constexpr operator Value() const noexcept { return m_val; }
    constexpr Value value() const noexcept { return m_val; }

    constexpr bool isSuccess() const noexcept { return m_val == Value::SUCCESS; }

    static LoginStatus fromString(std::string_view s);
    std::string str() const;

    constexpr bool operator==(LoginStatus other) const noexcept { return m_val == other.m_val; }
    constexpr bool operator!=(LoginStatus other) const noexcept { return m_val != other.m_val; }

    static const LoginStatus Unknown;
    static const LoginStatus Success;
    static const LoginStatus NeedCredential;
    static const LoginStatus NeedInteraction;
    static const LoginStatus Failed;
    static const LoginStatus Unsupported;
};

/** Effect of an access control entry. */
class AccessEffect {
  public:
    enum class Value {
        UNKNOWN = 0,
        ALLOW,
        DENY,
    };

  private:
    Value m_val = Value::UNKNOWN;

  public:
    constexpr AccessEffect() = default;
    constexpr AccessEffect(Value v) : m_val(v) {}

    constexpr operator Value() const noexcept { return m_val; }
    constexpr Value value() const noexcept { return m_val; }

    constexpr bool isAllow() const noexcept { return m_val == Value::ALLOW; }
    constexpr bool isDeny() const noexcept { return m_val == Value::DENY; }
    constexpr bool isUnknown() const noexcept { return m_val == Value::UNKNOWN; }

    static AccessEffect fromString(std::string_view s);
    std::string str() const;

    constexpr bool operator==(AccessEffect other) const noexcept { return m_val == other.m_val; }
    constexpr bool operator!=(AccessEffect other) const noexcept { return m_val != other.m_val; }

    static const AccessEffect Unknown;
    static const AccessEffect Allow;
    static const AccessEffect Deny;
};

} // namespace bas::security

#endif
