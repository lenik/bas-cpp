#include "path.hpp"

#include "strings.hpp"

#include <cctype>
#include <string>

namespace bas::reg {

PathStruct::PathStruct(const std::string& path) {
    // find last index of '/'  in path
    size_t lastSlash = path.find_last_of('/');
    std::string qname;
    if (lastSlash != std::string::npos) {
        auto left = path.substr(0, lastSlash);
        lastSlash = left.find_last_of('/');
        if (lastSlash != std::string::npos) {
            this->dirSegments = bas::util::split(left.substr(lastSlash + 1), '.');
            left = left.substr(lastSlash + 1);
        }
        this->baseName = left;
        qname = path.substr(lastSlash + 1);
    } else {
        qname = path;
    }

    size_t lastDot = qname.find_last_of('.');
    if (lastDot != std::string::npos) {
        auto domain = qname.substr(0, lastDot);
        this->domainSegments = bas::util::split(domain, '.');
        this->simpleKey = qname.substr(lastDot + 1);
    } else {
        this->simpleKey = qname;
    }
}

PathStruct::PathStruct(const PathStruct& other) {
    this->dirSegments = other.dirSegments;
    this->baseName = other.baseName;
    this->domainSegments = other.domainSegments;
    this->simpleKey = other.simpleKey;
}

PathStruct& PathStruct::operator=(const PathStruct& other) {
    this->dirSegments = other.dirSegments;
    this->baseName = other.baseName;
    this->domainSegments = other.domainSegments;
    this->simpleKey = other.simpleKey;
    return *this;
}

PathStruct& PathStruct::operator=(PathStruct&& other) noexcept {
    this->dirSegments = std::move(other.dirSegments);
    this->baseName = std::move(other.baseName);
    this->domainSegments = std::move(other.domainSegments);
    this->simpleKey = std::move(other.simpleKey);
    return *this;
}

bool PathStruct::operator==(const PathStruct& other) const {
    return this->dirSegments == other.dirSegments && this->baseName == other.baseName &&
           this->domainSegments == other.domainSegments && this->simpleKey == other.simpleKey;
}

bool PathStruct::operator!=(const PathStruct& other) const { return !(*this == other); }

bool PathStruct::operator<(const PathStruct& other) const {
    return this->dirSegments < other.dirSegments && this->baseName < other.baseName &&
           this->domainSegments < other.domainSegments && this->simpleKey < other.simpleKey;
}

bool PathStruct::operator<=(const PathStruct& other) const {
    return this->dirSegments <= other.dirSegments && this->baseName <= other.baseName &&
           this->domainSegments <= other.domainSegments && this->simpleKey <= other.simpleKey;
}

bool PathStruct::operator>(const PathStruct& other) const { return other < *this; }

bool PathStruct::operator>=(const PathStruct& other) const { return other <= *this; }

std::string PathStruct::str() const {
    std::string s;
    if (!this->dirSegments.empty()) {
        s = bas::util::join(this->dirSegments, '/') + "/";
    }
    if (this->baseName.has_value()) {
        s += this->baseName.value() + "/";
    }
    if (!this->domainSegments.empty()) {
        s += bas::util::join(this->domainSegments, '.') + ".";
    }
    s += this->simpleKey;
    return s;
}

PathStruct PathStruct::parse(const std::string& path) { return PathStruct(path); }

} // namespace bas::reg