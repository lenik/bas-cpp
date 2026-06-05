#ifndef BAS_SECURITY_DEMO_HPP
#define BAS_SECURITY_DEMO_HPP

#include "UserStore.hpp"
#include "PolicyStore.hpp"

namespace bas::security {

/** Fill @a store with built-in sample users (alice, bob, j, admin). */
void populateDemoUsers(DefaultUserStore& store);

/** DefaultUserStore preloaded with @ref populateDemoUsers. */
class DemoUserStore : public DefaultUserStore {
  public:
    DemoUserStore() { populateDemoUsers(*this); }

    std::string storeLabel() const override;
};

void populateDemoPolicy(PolicyStore& store);

class DemoPolicyStore : public DefaultPolicyStore {
  public:
    DemoPolicyStore() { populateDemoPolicy(*this); }
};

} // namespace bas::security

#endif // BAS_SECURITY_DEMO_HPP