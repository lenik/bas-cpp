#ifndef BAS_SECURITY_PUBLIC_ACCESS_HPP
#define BAS_SECURITY_PUBLIC_ACCESS_HPP

#include "PolicyStore.hpp"
#include "UserStore.hpp"

#include <memory>

namespace bas::security {

/**
 * Default open-access stores for volumes without ACL configuration.
 *
 * - UserStore: placeholder anonymous profile (login uses AnonymousIdentityService)
 * - PolicyStore: allow-all grants for anonymous/public identities
 */
class PublicAccess {
  public:
    static std::shared_ptr<UserStore> userStore();
    static std::shared_ptr<PolicyStore> policyStore();

    /** True if @a store is the shared PublicAccess user store. */
    static bool isPublicUserStore(const std::shared_ptr<UserStore>& store);
    /** True if @a store is the shared PublicAccess policy store. */
    static bool isPublicPolicyStore(const std::shared_ptr<PolicyStore>& store);

  private:
    PublicAccess() = delete;
};

} // namespace bas::security

#endif
