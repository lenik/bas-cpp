#include "Realm.hpp"

#include "bas/script/json.hpp"

#include <sstream>

namespace bas::security {

const Realm Realm::GLOBAL = {"global", "", "", "", ""};

bool Realm::matchType(const Realm& hint) const {
    if (hint.type.empty()) {
        return true;
    }
    return type == hint.type;
}

bool Realm::match(const Realm& hint) const {
    if (hint.empty() || !hint.hasKey()) {
        return true;
    }
    if (!matchType(hint)) {
        return false;
    }
    if (!hint.uuid.empty()) {
        return uuid == hint.uuid;
    }
    if (!hint.name.empty()) {
        return name == hint.name;
    }
    if (!hint.type.empty()) {
        return type == hint.type;
    }
    return true;
}

bool Realm::same(const Realm& o) const {
    if (this->empty() || o.empty()) {
        return true;
    }
    if (!this->type.empty() && !o.type.empty() && this->type != o.type) {
        return false;
    }
    if (!this->uuid.empty() && !o.uuid.empty()) {
        return this->uuid == o.uuid;
    }
    if (!this->name.empty() && !o.name.empty()) {
        return this->name == o.name;
    }
    if (!this->type.empty() && !o.type.empty()) {
        return this->type == o.type;
    }
    return false;
}

bool Realm::scopedEqual(const Realm& o) const {
    if (!this->type.empty() && !o.type.empty() && this->type != o.type) {
        return false;
    }
    if (!this->uuid.empty() && !o.uuid.empty()) {
        return this->uuid == o.uuid;
    }
    if (!this->name.empty() && !o.name.empty()) {
        return this->name == o.name;
    }
    if (!this->type.empty() && !o.type.empty()) {
        return this->type == o.type;
    }
    return this->empty() && o.empty();
}

std::string Realm::storageKey() const {
    if (!uuid.empty()) {
        return "uuid:" + uuid;
    }
    if (!type.empty() && !name.empty()) {
        return type + ":" + name;
    }
    if (!name.empty()) {
        return "name:" + name;
    }
    if (!type.empty()) {
        return "type:" + type;
    }
    return "global";
}

std::string Realm::displayLabel() const {
    if (!type.empty() && !name.empty()) {
        return type + ':' + name;
    }
    if (!name.empty()) {
        return name;
    }
    if (!type.empty()) {
        return type;
    }
    if (!uuid.empty()) {
        return uuid;
    }
    return {};
}

// Example:
// app:demoapp uuid=1234567890
std::string Realm::str() const {
    if (empty()) {
        return "(none)";
    }

    std::stringstream out;
    out << type << ':' << name;
    if (!uuid.empty()) {
        out << " uuid=" << uuid;
    }
    return out.str();
}

void Realm::jsonIn(const boost::json::value& in, const JsonFormOptions& /*opts*/) {
    if (in.is_string()) {
        *this = Realm{};
        name = in.as_string().c_str();
        return;
    }
    if (!in.is_object()) {
        *this = Realm{};
        return;
    }
    const auto& o = in.as_object();
    type = boost::json::get_string(o, "type", "");
    uuid = boost::json::get_string(o, "uuid", "");
    name = boost::json::get_string(o, "name", "");
    description = boost::json::get_string(o, "description", "");
    image = boost::json::get_string(o, "image", "");
}

void Realm::jsonOut(boost::json::object& o, const JsonFormOptions& /*opts*/) const {
    if (empty()) {
        return;
    }
    if (!type.empty()) {
        o["type"] = type;
    }
    if (!uuid.empty()) {
        o["uuid"] = uuid;
    }
    if (!name.empty()) {
        o["name"] = name;
    }
    if (!description.empty()) {
        o["description"] = description;
    }
    if (!image.empty()) {
        o["image"] = image;
    }
}

} // namespace bas::security
