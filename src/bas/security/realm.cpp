#include "realm.hpp"

namespace bas::security {

namespace {

bool realmTypeMatches(const Realm& stored, const Realm& hint) {
    if (hint.type.empty()) {
        return true;
    }
    return stored.type == hint.type;
}

} // namespace

const Realm Realm::GLOBAL = {"global", "", "", "", ""};

bool realmMatches(const Realm& stored, const Realm& hint) {
    if (hint.empty() || !hint.hasKey()) {
        return true;
    }
    if (!realmTypeMatches(stored, hint)) {
        return false;
    }
    if (!hint.uuid.empty()) {
        return stored.uuid == hint.uuid;
    }
    if (!hint.name.empty()) {
        return stored.name == hint.name;
    }
    if (!hint.type.empty()) {
        return stored.type == hint.type;
    }
    return true;
}

bool realmSame(const Realm& a, const Realm& b) {
    if (a.empty() || b.empty()) {
        return true;
    }
    if (!a.type.empty() && !b.type.empty() && a.type != b.type) {
        return false;
    }
    if (!a.uuid.empty() && !b.uuid.empty()) {
        return a.uuid == b.uuid;
    }
    if (!a.name.empty() && !b.name.empty()) {
        return a.name == b.name;
    }
    if (!a.type.empty() && !b.type.empty()) {
        return a.type == b.type;
    }
    return false;
}

bool realmScopedEqual(const Realm& a, const Realm& b) {
    if (!a.type.empty() && !b.type.empty() && a.type != b.type) {
        return false;
    }
    if (!a.uuid.empty() && !b.uuid.empty()) {
        return a.uuid == b.uuid;
    }
    if (!a.name.empty() && !b.name.empty()) {
        return a.name == b.name;
    }
    if (!a.type.empty() && !b.type.empty()) {
        return a.type == b.type;
    }
    return a.empty() && b.empty();
}

std::string realmStorageKey(const Realm& realm) {
    if (!realm.uuid.empty()) {
        return "uuid:" + realm.uuid;
    }
    if (!realm.type.empty() && !realm.name.empty()) {
        return realm.type + ":" + realm.name;
    }
    if (!realm.name.empty()) {
        return "name:" + realm.name;
    }
    if (!realm.type.empty()) {
        return "type:" + realm.type;
    }
    return "global";
}

std::string realmDisplayLabel(const Realm& realm) {
    if (!realm.type.empty() && !realm.name.empty()) {
        return realm.type + ':' + realm.name;
    }
    if (!realm.name.empty()) {
        return realm.name;
    }
    if (!realm.type.empty()) {
        return realm.type;
    }
    if (!realm.uuid.empty()) {
        return realm.uuid;
    }
    return {};
}

} // namespace bas::security
