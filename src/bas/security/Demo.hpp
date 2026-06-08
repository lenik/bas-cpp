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

/** Built-in users for devdemo tank-a (alice/operator, bob/gunner, admin/commander). */
void populateTankAUsers(DefaultUserStore& store);

class TankAUserStore : public DefaultUserStore {
  public:
    TankAUserStore() { populateTankAUsers(*this); }
};

void populateTankAPolicy(PolicyStore& store);

class TankAPolicyStore : public DefaultPolicyStore {
  public:
    TankAPolicyStore() { populateTankAPolicy(*this); }
};

/** Built-in users for devdemo tank-b (charlie/cadet, dana/instructor). */
void populateTankBUsers(DefaultUserStore& store);

class TankBUserStore : public DefaultUserStore {
  public:
    TankBUserStore() { populateTankBUsers(*this); }
};

void populateTankBPolicy(PolicyStore& store);

class TankBPolicyStore : public DefaultPolicyStore {
  public:
    TankBPolicyStore() { populateTankBPolicy(*this); }
};

} // namespace bas::security

#endif // BAS_SECURITY_DEMO_HPP