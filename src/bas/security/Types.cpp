#include "Types.hpp"

namespace bas::security {

const IdentityState IdentityState::Unknown{IdentityState::Value::UNKNOWN};
const IdentityState IdentityState::Active{IdentityState::Value::ACTIVE};
const IdentityState IdentityState::Expired{IdentityState::Value::EXPIRED};
const IdentityState IdentityState::Revoked{IdentityState::Value::REVOKED};
const IdentityState IdentityState::LoggedOut{IdentityState::Value::LOGGED_OUT};

const IdentitySource IdentitySource::Unknown{IdentitySource::Value::UNKNOWN};
const IdentitySource IdentitySource::Direct{IdentitySource::Value::DIRECT};
const IdentitySource IdentitySource::Derived{IdentitySource::Value::DERIVED};
const IdentitySource IdentitySource::Login{IdentitySource::Value::LOGIN};
const IdentitySource IdentitySource::Auto{IdentitySource::Value::AUTO};
const IdentitySource IdentitySource::System{IdentitySource::Value::SYSTEM};

const LoginStatus LoginStatus::Unknown{LoginStatus::Value::UNKNOWN};
const LoginStatus LoginStatus::Success{LoginStatus::Value::SUCCESS};
const LoginStatus LoginStatus::NeedCredential{LoginStatus::Value::NEED_CREDENTIAL};
const LoginStatus LoginStatus::NeedInteraction{LoginStatus::Value::NEED_INTERACTION};
const LoginStatus LoginStatus::Failed{LoginStatus::Value::FAILED};
const LoginStatus LoginStatus::Unsupported{LoginStatus::Value::UNSUPPORTED};

const AccessEffect AccessEffect::Unknown{AccessEffect::Value::UNKNOWN};
const AccessEffect AccessEffect::Allow{AccessEffect::Value::ALLOW};
const AccessEffect AccessEffect::Deny{AccessEffect::Value::DENY};

IdentityState IdentityState::fromString(std::string_view s) {
    if (s == "active") {
        return Active;
    }
    if (s == "expired") {
        return Expired;
    }
    if (s == "revoked") {
        return Revoked;
    }
    if (s == "logged_out") {
        return LoggedOut;
    }
    return Unknown;
}

std::string IdentityState::str() const {
    switch (m_val) {
    case Value::ACTIVE:
        return "active";
    case Value::EXPIRED:
        return "expired";
    case Value::REVOKED:
        return "revoked";
    case Value::LOGGED_OUT:
        return "logged_out";
    default:
        return "unknown";
    }
}

int IdentitySource::priority() const noexcept {
    switch (m_val) {
    case Value::DIRECT:
        return 4;
    case Value::LOGIN:
        return 4;
    case Value::DERIVED:
        return 3;
    case Value::AUTO:
        return 2;
    case Value::SYSTEM:
        return 1;
    default:
        return 0;
    }
}

IdentitySource IdentitySource::fromString(std::string_view s) {
    if (s == "direct") {
        return Direct;
    }
    if (s == "derived") {
        return Derived;
    }
    if (s == "login") {
        return Login;
    }
    if (s == "auto") {
        return Auto;
    }
    if (s == "system") {
        return System;
    }
    return Unknown;
}

std::string IdentitySource::str() const {
    switch (m_val) {
    case Value::DIRECT:
        return "direct";
    case Value::DERIVED:
        return "derived";
    case Value::LOGIN:
        return "login";
    case Value::AUTO:
        return "auto";
    case Value::SYSTEM:
        return "system";
    default:
        return "unknown";
    }
}

LoginStatus LoginStatus::fromString(std::string_view s) {
    if (s == "success") {
        return Success;
    }
    if (s == "need_credential") {
        return NeedCredential;
    }
    if (s == "need_interaction") {
        return NeedInteraction;
    }
    if (s == "failed") {
        return Failed;
    }
    if (s == "unsupported") {
        return Unsupported;
    }
    return Unknown;
}

std::string LoginStatus::str() const {
    switch (m_val) {
    case Value::SUCCESS:
        return "success";
    case Value::NEED_CREDENTIAL:
        return "need_credential";
    case Value::NEED_INTERACTION:
        return "need_interaction";
    case Value::FAILED:
        return "failed";
    case Value::UNSUPPORTED:
        return "unsupported";
    default:
        return "unknown";
    }
}

AccessEffect AccessEffect::fromString(std::string_view s) {
    if (s == "allow") {
        return Allow;
    }
    if (s == "deny") {
        return Deny;
    }
    return Unknown;
}

std::string AccessEffect::str() const {
    switch (m_val) {
    case Value::ALLOW:
        return "allow";
    case Value::DENY:
        return "deny";
    default:
        return "unknown";
    }
}

} // namespace bas::security
