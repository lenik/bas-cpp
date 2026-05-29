#ifndef BAS_SECURITY_TYPES_HPP
#define BAS_SECURITY_TYPES_HPP

#include <boost/json.hpp>

#include <string>

namespace bas::security {

using Permission = std::string;
using QualifiedToken = std::string;
using JsonObject = boost::json::object;

enum class ACMode {
    Unknown = 0,
    Allow,
    Deny
};

enum class IdentityState {
    Unknown = 0,
    Active,
    Expired,
    Revoked,
    LoggedOut
};

enum class IdentitySource {
    Unknown = 0,
    Direct,
    Derived,
    Auto,
    System
};

enum class LoginStatus {
    Unknown = 0,
    Success,
    NeedCredential,
    NeedInteraction,
    Failed,
    Unsupported
};

enum class LoginConflictAction {
    KeepExisting = 0,
    ReplaceExisting,
    RejectIncoming,
    AskUser
};

} // namespace bas::security

#endif
