#ifndef BAS_SECURITY_AC_LIST_HPP
#define BAS_SECURITY_AC_LIST_HPP

#include "CommandSupport.hpp"
#include "Permission.hpp"
#include "Types.hpp"

#include "../fmt/JsonForm.hpp"

#include <string>
#include <vector>

namespace bas::security {

/** Pure ACE: permission + effect, no identity binding. */
struct ACEntry {
    Permission permission;
    AccessEffect effect = AccessEffect::Unknown;

    bool operator==(const ACEntry& other) const {
        return permission == other.permission && effect == other.effect;
    }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT);
    void jsonOut(boost::json::object& o, const JsonFormOptions& opts = JsonFormOptions::DEFAULT) const;
};

/** Named ACE list: pure permission + effect rows without identity binding. */
class ACList : public IJsonForm, public ICommandSupport {
  public:
    std::string id;
    std::vector<ACEntry> entries;

    bool operator==(const ACList& other) const {
        return id == other.id && entries == other.entries;
    }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts) override;

    void jsonOut(boost::json::object& o, const JsonFormOptions& opts) override;

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;
};

} // namespace bas::security

#endif
