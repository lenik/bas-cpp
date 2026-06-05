#include "PasswordDigest.hpp"

#include <boost/json.hpp>

#include <openssl/evp.h>

#include <cctype>
#include <stdexcept>
#include <vector>

namespace bas::security {

namespace {

const EVP_MD* digestForAlgorithm(std::string_view algorithm) {
    if (algorithm == "sha256" || algorithm == "SHA256") {
        return EVP_sha256();
    }
    if (algorithm == "sha1" || algorithm == "SHA1") {
        return EVP_sha1();
    }
    if (algorithm == "md5" || algorithm == "MD5") {
        return EVP_md5();
    }
    return nullptr;
}

std::string toLowerAscii(std::string_view text) {
    std::string out(text);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string bytesToHex(const unsigned char* data, std::size_t len) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0x0f]);
        out.push_back(kHex[data[i] & 0x0f]);
    }
    return out;
}

bool secureEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string jsonStringField(const JsonObject& data, const char* key) {
    if (auto* v = data.if_contains(key)) {
        if (v->is_string()) {
            return std::string(v->as_string().c_str());
        }
    }
    return {};
}

bool verifyPasswordHash(std::string_view password, const UserAuthKey& key) {
    const std::string algorithm = toLowerAscii(jsonStringField(key.data, "algorithm"));
    const std::string storedHash = jsonStringField(key.data, "hash");
    if (algorithm.empty() || storedHash.empty()) {
        return false;
    }

    // Legacy demo files: algorithm demo-plain stored cleartext in hash.
    if (algorithm == "demo-plain") {
        return secureEqual(password, storedHash);
    }

    if (!isSupportedPasswordHashAlgorithm(algorithm)) {
        return false;
    }

    const std::string computed = digestPassword(algorithm, password);
    return secureEqual(computed, storedHash);
}

bool verifyPasswordPlain(std::string_view password, const UserAuthKey& key) {
    const std::string stored = jsonStringField(key.data, "password");
    if (stored.empty()) {
        return false;
    }
    return secureEqual(password, stored);
}

} // namespace

std::vector<std::string> supportedPasswordHashAlgorithms() {
    return {"sha256", "sha1", "md5"};
}

bool isSupportedPasswordHashAlgorithm(std::string_view algorithm) {
    return digestForAlgorithm(algorithm) != nullptr;
}

std::string digestPassword(std::string_view algorithm, std::string_view password) {
    const EVP_MD* md = digestForAlgorithm(algorithm);
    if (!md) {
        throw std::invalid_argument("unsupported password hash algorithm: " + std::string(algorithm));
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("EVP_MD_CTX_new failed");
    }

    std::vector<unsigned char> digest(static_cast<std::size_t>(EVP_MD_size(md)));
    unsigned int digestLen = 0;

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx, password.data(), password.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest.data(), &digestLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("password digest failed");
    }
    EVP_MD_CTX_free(ctx);

    return bytesToHex(digest.data(), digestLen);
}

bool passwordMatchesAuthKey(std::string_view password, const UserAuthKey& key,
                            std::chrono::system_clock::time_point now) {
    if (!key.enabled || key.isExpired(now)) {
        return false;
    }
    if (key.type == "password-hash") {
        return verifyPasswordHash(password, key);
    }
    if (key.type == "password-plain") {
        return verifyPasswordPlain(password, key);
    }
    return false;
}

UserAuthKey makePasswordHashKey(const std::string& id, std::string_view password,
                                std::string_view algorithm) {
    const std::string algo = toLowerAscii(algorithm);
    UserAuthKey key;
    key.id = id;
    key.type = "password-hash";
    key.name = "Main Password";
    key.enabled = true;
    key.createdAt = std::chrono::system_clock::now();
    key.data["algorithm"] = boost::json::value(algo);
    key.data["hash"] = boost::json::value(digestPassword(algo, password));
    return key;
}

bool isPasswordStorageAlgorithm(std::string_view algorithm) {
    const std::string algo = toLowerAscii(algorithm);
    if (algo == "plain") {
        return true;
    }
    return isSupportedPasswordHashAlgorithm(algo);
}

UserAuthKey makePasswordKeyForAlgorithm(const std::string& id, std::string_view password,
                                        std::string_view algorithm) {
    const std::string algo = toLowerAscii(algorithm);
    if (algo == "plain") {
        return makePasswordPlainKey(id, password);
    }
    return makePasswordHashKey(id, password, algo);
}

UserAuthKey makePasswordPlainKey(const std::string& id, std::string_view password) {
    UserAuthKey key;
    key.id = id;
    key.type = "password-plain";
    key.name = "Main Password";
    key.enabled = true;
    key.createdAt = std::chrono::system_clock::now();
    key.data["password"] = boost::json::value(std::string(password));
    return key;
}

} // namespace bas::security
