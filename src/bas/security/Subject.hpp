#ifndef BAS_SECURITY_SUBJECT_HPP
#define BAS_SECURITY_SUBJECT_HPP

#include "Identity.hpp"

#include <vector>

namespace bas::security {

/** Active identities attached to a request (Subject = IdentitySet at authorization time). */
struct Subject {
    std::vector<Identity> identities;

    bool empty() const { return identities.empty(); }

    std::vector<IdentityRef> refs() const {
        std::vector<IdentityRef> out;
        out.reserve(identities.size());
        for (const auto& id : identities) {
            out.push_back(id.ref());
        }
        return out;
    }

    static Subject fromIdentities(std::vector<Identity> ids) {
        Subject subject;
        subject.identities = std::move(ids);
        return subject;
    }

    static Subject fromIdentitySet(const IdentitySet& set) {
        Subject subject;
        if (set.primary.has_value()) {
            subject.identities.push_back(*set.primary);
        }
        for (const auto& id : set.identities) {
            if (set.primary.has_value() && id.ref() == set.primary->ref()) {
                continue;
            }
            subject.identities.push_back(id);
        }
        return subject;
    }
};

} // namespace bas::security

#endif
