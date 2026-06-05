#include "CredentialManager.hpp"

#include "../time/Instant.hpp"
#include "../time/OffsetDateTime.hpp"
#include "../time/ZonedDateTime.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace bas::security {

void Credential::jsonIn(const boost::json::object& in, const JsonFormOptions& opts) {
    ref.jsonIn(in, opts);
    meta.jsonIn(in, opts);

    if (auto* s = in.if_contains("secret")) {
        if (s->is_string()) {
            secret = SecretValue(boost::json::value_to<std::string>(*s));
        }
    }

    if (auto* d = in.if_contains("publicData")) {
        if (d->is_object()) {
            publicData = boost::json::value_to<JsonObject>(*d);
        }
    }
}

void Credential::jsonOut(boost::json::object& out, const JsonFormOptions& opts) const {
    ref.jsonOut(out, opts);
    meta.jsonOut(out, opts);
    if (secret.has_value()) {
        out["secret"] = secret->value();
    }
    if (!publicData.empty()) {
        out["publicData"] = publicData;
    }
}

void CredentialRef::jsonIn(const boost::json::object& in, const JsonFormOptions& opts) {
    store = boost::json::value_to<std::string>(in.at("store"));
    id = boost::json::value_to<std::string>(in.at("id"));
}

void CredentialRef::jsonOut(boost::json::object& out, const JsonFormOptions& opts) const {
    out["store"] = store;
    out["id"] = id;
}

void CredentialMeta::jsonIn(const boost::json::object& in, const JsonFormOptions& opts) {
    type = boost::json::value_to<std::string>(in.at("type"));
    name = boost::json::value_to<std::string>(in.at("name"));
    subjectHint = boost::json::value_to<std::string>(in.at("subjectHint"));
    serviceHint = boost::json::value_to<std::string>(in.at("serviceHint"));
    if (auto* realmVal = in.if_contains("realm")) {
        realm.jsonIn(*realmVal, opts);
    }
    if (auto* expires = in.if_contains("expiresAt")) {
        std::string expiresStr = boost::json::value_to<std::string>(*expires);
        if (time::Instant::isValidIsoString(expiresStr))
            expiresAt = time::Instant::parseIsoString(expiresStr)->toTimePoint();
        else if (time::ZonedDateTime::isValidIsoString(expiresStr))
            expiresAt = time::ZonedDateTime::parseIsoString(expiresStr)->toTimePoint();
        else if (time::OffsetDateTime::isValidIsoString(expiresStr))
            expiresAt = time::OffsetDateTime::parseIsoString(expiresStr)->toTimePoint();
        else
            expiresAt = std::nullopt;
    }
    if (auto* a = in.if_contains("attributes")) {
        if (a->is_object()) {
            attributes = a->as_object();
        }
    }
}

void CredentialMeta::jsonOut(boost::json::object& out, const JsonFormOptions& opts) const {
    out["type"] = type;
    out["name"] = name;
    out["subjectHint"] = subjectHint;
    out["serviceHint"] = serviceHint;
    if (!realm.empty()) {
        boost::json::object realmObj;
        realm.jsonOut(realmObj, opts);
        out["realm"] = realmObj;
    }
    if (expiresAt.has_value()) {
        auto zdt = time::ZonedDateTime::fromTimePoint(*expiresAt);
        out["expiresAt"] = zdt->toIsoString();
    }
    if (!attributes.empty()) {
        out["attributes"] = attributes;
    }
}

SecretValue::SecretValue(std::string value) : m_value(std::move(value)) {}

SecretValue::SecretValue(SecretValue&& other) noexcept : m_value(std::move(other.m_value)) {
    other.m_value.clear();
}

SecretValue& SecretValue::operator=(SecretValue&& other) noexcept {
    if (this != &other) {
        clear();
        m_value = std::move(other.m_value);
        other.m_value.clear();
    }
    return *this;
}

SecretValue::~SecretValue() { clear(); }

const std::string& SecretValue::value() const { return m_value; }

std::string SecretValue::masked() const {
    if (m_value.empty()) {
        return "";
    }
    if (m_value.size() <= 4) {
        return "****";
    }
    return std::string(m_value.size() - 4, '*') + m_value.substr(m_value.size() - 4);
}

void SecretValue::clear() {
    if (!m_value.empty()) {
        std::fill(m_value.begin(), m_value.end(), '\0');
        m_value.clear();
    }
}

namespace {

bool refMatches(const CredentialRef& a, const CredentialRef& b) {
    return a.store == b.store && a.id == b.id;
}

bool credentialTypeMatches(const Credential& cred, const CredentialRequest& request) {
    if (request.types.empty()) {
        return true;
    }
    return std::find(request.types.begin(), request.types.end(), cred.meta.type) !=
           request.types.end();
}

bool credentialUsable(const Credential& cred, const CredentialRequest& request) {
    if (!request.subjectHint.empty() && !cred.meta.subjectHint.empty() &&
        request.subjectHint != cred.meta.subjectHint) {
        return false;
    }
    if (!request.serviceHint.empty() && !cred.meta.serviceHint.empty() &&
        request.serviceHint != cred.meta.serviceHint) {
        return false;
    }
    if (!cred.meta.realm.match(request.realmHint)) {
        return false;
    }
    const auto now = std::chrono::system_clock::now();
    return !cred.meta.isExpired(now) && credentialTypeMatches(cred, request);
}

Credential cloneCredential(const Credential& cred) {
    Credential out;
    out.ref = cred.ref;
    out.meta = cred.meta;
    out.publicData = cred.publicData;
    if (cred.secret.has_value()) {
        out.secret = SecretValue(cred.secret->value());
    }
    return out;
}

} // namespace

std::optional<Credential> DefaultCredentialManager::get(const CredentialRequest& request) {
    for (const auto& cred : m_credentials) {
        if (!credentialUsable(cred, request)) {
            continue;
        }
        return cloneCredential(cred);
    }
    return std::nullopt;
}

CredentialRef DefaultCredentialManager::put(Credential credential) {
    if (credential.ref.store.empty()) {
        credential.ref.store = kStore;
    }
    if (credential.ref.id.empty()) {
        credential.ref.id = "cred-" + std::to_string(m_nextId++);
    }
    const CredentialRef ref = credential.ref;
    remove(ref);
    m_credentials.push_back(std::move(credential));
    return ref;
}

std::optional<Credential> DefaultCredentialManager::load(const CredentialRef& ref) {
    for (const auto& cred : m_credentials) {
        if (refMatches(cred.ref, ref)) {
            return cloneCredential(cred);
        }
    }
    return std::nullopt;
}

void DefaultCredentialManager::remove(const CredentialRef& ref) {
    m_credentials.erase(std::remove_if(m_credentials.begin(), m_credentials.end(),
                                       [&](const Credential& c) { return refMatches(c.ref, ref); }),
                        m_credentials.end());
}

void DefaultCredentialManager::clear() { m_credentials.clear(); }

std::vector<CredentialInfo> DefaultCredentialManager::list(const CredentialRequest& request) const {
    std::vector<CredentialInfo> infos;
    for (const auto& cred : m_credentials) {
        if (!credentialUsable(cred, request)) {
            continue;
        }
        infos.push_back(CredentialInfo{cred.ref, cred.meta});
    }
    return infos;
}

void DefaultCredentialManager::jsonIn(const boost::json::value& in, const JsonFormOptions& opts) {
    if (in.is_null()) {
        return;
    }
    if (in.is_array()) {
        m_credentials.clear();
        for (const auto& item : in.as_array()) {
            if (!item.is_object()) {
                continue;
            }
            Credential cred;
            cred.jsonIn(item.as_object(), opts);
            put(std::move(cred));
        }
        return;
    }
    if (in.is_object()) {
        jsonIn(in.as_object(), opts);
    }
}

void DefaultCredentialManager::jsonIn(const boost::json::object& o, const JsonFormOptions& opts) {
    m_credentials.clear();
    if (auto* items = o.if_contains("credentials")) {
        if (!items->is_array()) {
            return;
        }
        jsonIn(items->as_array(), opts);
    }
}

boost::json::value DefaultCredentialManager::jsonOut(const JsonFormOptions& opts) {
    boost::json::array array;
    for (const auto& cred : m_credentials) {
        boost::json::object item;
        cred.jsonOut(item, opts);
        array.push_back(item);
    }
    return array;
}

void DefaultCredentialManager::jsonOut(boost::json::object& out, const JsonFormOptions& opts) {
    auto array = jsonOut(opts);
    out["credentials"] = array;
}

std::optional<Credential> DecoratedCredentialManager::get(const CredentialRequest& request) {
    return m_wrapped->get(request);
}

CredentialRef DecoratedCredentialManager::put(Credential credential) {
    return m_wrapped->put(std::move(credential));
}

std::optional<Credential> DecoratedCredentialManager::load(const CredentialRef& ref) {
    return m_wrapped->load(ref);
}

std::vector<CredentialInfo>
DecoratedCredentialManager::list(const CredentialRequest& request) const {
    return m_wrapped->list(request);
}

void DecoratedCredentialManager::remove(const CredentialRef& ref) { m_wrapped->remove(ref); }

void DecoratedCredentialManager::clear() { m_wrapped->clear(); }

void DecoratedCredentialManager::jsonIn(const boost::json::value& in, const JsonFormOptions& opts) {
    m_wrapped->jsonIn(in, opts);
}

boost::json::value DecoratedCredentialManager::jsonOut(const JsonFormOptions& opts) {
    return m_wrapped->jsonOut(opts);
}

void DecoratedCredentialManager::jsonIn(const boost::json::object& in,
                                        const JsonFormOptions& opts) {
    m_wrapped->jsonIn(in, opts);
}

void DecoratedCredentialManager::jsonOut(boost::json::object& out, const JsonFormOptions& opts) {
    m_wrapped->jsonOut(out, opts);
}

FileCredentialManager::FileCredentialManager(std::filesystem::path path,
                                             std::shared_ptr<CredentialManager> wrapped)
    : DecoratedCredentialManager(std::move(wrapped)), m_path(std::move(path)) {
    loadFromDisk();
}

void FileCredentialManager::loadFromDisk() {
    m_loadedCount = 0;
    wrapped()->clear();
    if (!std::filesystem::exists(m_path)) {
        return;
    }

    std::ifstream in(m_path);
    if (!in) {
        return;
    }
    std::ostringstream content;
    content << in.rdbuf();
    if (content.str().empty()) {
        return;
    }

    boost::json::value root;
    try {
        root = boost::json::parse(content.str());
    } catch (const std::exception&) {
        return;
    }

    wrapped()->jsonIn(root, JsonFormOptions::DEFAULT);
    m_loadedCount = wrapped()->list(CredentialRequest{}).size();
}

void FileCredentialManager::saveToDisk() {
    if (m_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(m_path.parent_path(), ec);
    }

    auto root = wrapped()->jsonOut(JsonFormOptions::DEFAULT);
    std::ofstream out(m_path);
    if (!out) {
        throw std::runtime_error("failed to write credentials: " + m_path.string());
    }
    out << boost::json::serialize(root);
}

std::optional<Credential> FileCredentialManager::get(const CredentialRequest& request) {
    return wrapped()->get(request);
}

CredentialRef FileCredentialManager::put(Credential credential) {
    credential.ref.store = kStore;
    const auto ref = wrapped()->put(std::move(credential));
    saveToDisk();
    return ref;
}

std::optional<Credential> FileCredentialManager::load(const CredentialRef& ref) {
    return wrapped()->load(ref);
}

std::vector<CredentialInfo> FileCredentialManager::list(const CredentialRequest& request) const {
    return wrapped()->list(request);
}

void FileCredentialManager::remove(const CredentialRef& ref) {
    wrapped()->remove(ref);
    saveToDisk();
}

void FileCredentialManager::clear() {
    wrapped()->clear();
    m_loadedCount = 0;
    saveToDisk();
}

void FileCredentialManager::flush() { saveToDisk(); }

void FileCredentialManager::reload() { loadFromDisk(); }

} // namespace bas::security
