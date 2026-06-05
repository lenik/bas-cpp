#include "LoginUi.hpp"

#include "SecurityManager.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace bas::security {

namespace {

std::string trimCopy(std::string s) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

#if defined(__unix__) || defined(__APPLE__)
bool readLineFromStdin(const std::string& prompt, bool secret, std::string& out) {
    std::cerr << prompt << std::flush;
    if (!secret) {
        if (!std::getline(std::cin, out)) {
            return false;
        }
        out = trimCopy(std::move(out));
        return true;
    }

    termios oldState{};
    if (tcgetattr(STDIN_FILENO, &oldState) != 0) {
        if (!std::getline(std::cin, out)) {
            return false;
        }
        out = trimCopy(std::move(out));
        std::cerr << '\n';
        return true;
    }

    termios newState = oldState;
    newState.c_lflag &= static_cast<tcflag_t>(~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newState);
    const bool ok = static_cast<bool>(std::getline(std::cin, out));
    tcsetattr(STDIN_FILENO, TCSANOW, &oldState);
    std::cerr << '\n';
    if (!ok) {
        return false;
    }
    out = trimCopy(std::move(out));
    return true;
}
#else
bool readLineFromStdin(const std::string& prompt, bool /*secret*/, std::string& out) {
    std::cerr << prompt << std::flush;
    if (!std::getline(std::cin, out)) {
        return false;
    }
    out = trimCopy(std::move(out));
    return true;
}
#endif

std::string fieldOptionString(const LoginField& field, const char* key) {
    const auto it = field.options.find(key);
    if (it == field.options.end() || !it->value().is_string()) {
        return {};
    }
    return std::string(it->value().as_string().c_str());
}

void finalizeCredentialRealm(Credential& cred, const CredentialRequest& request) {
    if (cred.meta.realm.type.empty() && !request.realmHint.type.empty()) {
        cred.meta.realm.type = request.realmHint.type;
    }
    if (cred.meta.realm.name.empty() && !request.realmHint.name.empty()) {
        cred.meta.realm.name = request.realmHint.name;
    }
    if (cred.meta.realm.uuid.empty() && !request.realmHint.uuid.empty()) {
        cred.meta.realm.uuid = request.realmHint.uuid;
    }
    if (cred.meta.realm.type.empty()) {
        cred.meta.realm.type = "device";
    }
}

} // namespace

std::optional<Credential> credentialFromLoginForm(
    const LoginFormSpec& form, const CredentialRequest& request,
    const std::function<std::optional<std::string>(const LoginField& field)>& readField) {
    Credential cred;
    cred.meta.serviceHint = request.serviceHint;
    if (!request.realmHint.empty()) {
        cred.meta.realm = request.realmHint;
    }
    if (!request.subjectHint.empty()) {
        cred.meta.subjectHint = request.subjectHint;
    }
    if (!request.types.empty()) {
        cred.meta.type = request.types.front();
    }

    for (const LoginField& field : form.fields) {
        const auto value = readField(field);
        if (!value.has_value()) {
            return std::nullopt;
        }
        if (field.required && value->empty()) {
            return std::nullopt;
        }

        if (field.type == "password") {
            if (!value->empty()) {
                cred.secret = SecretValue(*value);
            }
            continue;
        }

        if (field.name == "realm_type") {
            std::string chosen = value->empty() ? fieldOptionString(field, "value") : *value;
            if (!chosen.empty()) {
                cred.meta.realm.type = std::move(chosen);
            } else if (!request.realmHint.type.empty()) {
                cred.meta.realm.type = request.realmHint.type;
            }
            continue;
        }

        if (field.name == "realm") {
            std::string chosen = value->empty() ? fieldOptionString(field, "value") : *value;
            if (!chosen.empty()) {
                cred.meta.realm.name = std::move(chosen);
            } else if (!request.realmHint.name.empty()) {
                cred.meta.realm.name = request.realmHint.name;
            }
            continue;
        }

        if (field.name == "realm_uuid") {
            std::string chosen = value->empty() ? fieldOptionString(field, "value") : *value;
            if (!chosen.empty()) {
                cred.meta.realm.uuid = std::move(chosen);
            } else if (!request.realmHint.uuid.empty()) {
                cred.meta.realm.uuid = request.realmHint.uuid;
            }
            continue;
        }

        if (field.name == "username") {
            if (!value->empty()) {
                cred.meta.subjectHint = *value;
            }
            continue;
        }

        if (field.type == "text") {
            if (!value->empty()) {
                cred.meta.subjectHint = *value;
            }
            continue;
        }
    }

    if (!cred.secret.has_value()) {
        return std::nullopt;
    }
    if (cred.meta.type.empty()) {
        cred.meta.type = "password";
    }
    if (!form.title.empty()) {
        cred.meta.name = form.title;
    }
    finalizeCredentialRealm(cred, request);
    return cred;
}

ConsoleLogin::ConsoleLogin(SecurityManager& controller) : m_controller(controller) {}

SecurityManager* ConsoleLogin::controller() { return &m_controller; }

std::optional<std::string> ConsoleLogin::realmType() const {
    if (auto r = realm()) {
        if (!r->type.empty()) {
            return r->type;
        }
    }
    return std::nullopt;
}

std::optional<Realm> ConsoleLogin::realm() const {
    if (!m_controller.realms().empty()) {
        return m_controller.realms().front();
    }
    return std::nullopt;
}

std::optional<Credential> ConsoleLogin::requestCredential(const LoginFormSpec& form,
                                                          const CredentialRequest& request) {
    if (!form.title.empty()) {
        std::cerr << form.title << '\n';
    }

    return credentialFromLoginForm(form, request, [](const LoginField& field) -> std::optional<std::string> {
        std::string label = field.label.empty() ? field.name : field.label;
        if (label.empty()) {
            label = field.name;
        }
        if (!label.empty() && label.back() != ':' && label.back() != '?') {
            label += ": ";
        }

        const bool secret = field.type == "password" || field.name == "password";
        std::string value;
        if (!readLineFromStdin(label, secret, value)) {
            return std::nullopt;
        }
        return value;
    });
}

} // namespace bas::security
