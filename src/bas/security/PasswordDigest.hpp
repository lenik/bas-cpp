#ifndef BAS_SECURITY_PASSWORD_DIGEST_HPP
#define BAS_SECURITY_PASSWORD_DIGEST_HPP

#include "UserStore.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bas::security {

/** Supported digest names for @c password-hash keys (e.g. sha256, sha1, md5). */
std::vector<std::string> supportedPasswordHashAlgorithms();

bool isSupportedPasswordHashAlgorithm(std::string_view algorithm);

/** Compute hex digest of @a password using @a algorithm (sha256, sha1, md5). */
std::string digestPassword(std::string_view algorithm, std::string_view password);

/** Compare @a password to stored @c password-hash or @c password-plain key material. */
bool passwordMatchesAuthKey(std::string_view password, const UserAuthKey& key,
                            std::chrono::system_clock::time_point now);

/** Build a @c password-hash key; @a algorithm defaults to sha256. */
UserAuthKey makePasswordHashKey(const std::string& id, std::string_view password,
                                std::string_view algorithm = "sha256");

/** Build a @c password-plain key (stores cleartext in data.password — demo only). */
UserAuthKey makePasswordPlainKey(const std::string& id, std::string_view password);

/** @c plain → password-plain; sha256/sha1/md5 → password-hash. */
bool isPasswordStorageAlgorithm(std::string_view algorithm);

UserAuthKey makePasswordKeyForAlgorithm(const std::string& id, std::string_view password,
                                        std::string_view algorithm);

} // namespace bas::security

#endif
