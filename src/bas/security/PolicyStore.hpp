#ifndef BAS_SECURITY_POLICY_STORE_HPP
#define BAS_SECURITY_POLICY_STORE_HPP

#include "ACList.hpp"
#include "AccessDecisionResolver.hpp"
#include "Binding.hpp"
#include "CommandSupport.hpp"
#include "Identity.hpp"
#include "Permission.hpp"

#include "../fmt/JsonForm.hpp"
#include "../volume/LocalVolume.hpp"
#include "../volume/VolumeFile.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bas::security {

class PolicyStore : public IJsonForm, public ICommandSupport {
  public:
    virtual ~PolicyStore() = default;

    virtual bool hasAcl(const std::string& aclId) const = 0;

    virtual std::optional<ACList> getAcl(const std::string& aclId) const = 0;

    virtual std::vector<std::string> listAcls() const = 0;

    virtual void addAcl(ACList acl) = 0;

    virtual void updateAcl(ACList acl) = 0;

    virtual void removeAcl(const std::string& aclId) = 0;

    virtual std::vector<AccessGrant> grantsOf(const IdentityRef& identity) const = 0;

    virtual void addGrant(AccessGrant grant) = 0;

    virtual void removeGrant(const AccessGrant& grant) = 0;

    virtual std::vector<PolicyBinding> bindingsOf(const IdentityRef& identity) const = 0;

    virtual void addBinding(PolicyBinding binding) = 0;

    virtual void removeBinding(const PolicyBinding& binding) = 0;

    virtual std::vector<ACEntry> effectiveEntriesOf(const IdentityRef& identity) const = 0;

    virtual void clear() = 0;

    virtual std::filesystem::path storePath() const { return {}; }

    virtual bool canPersistToDisk() const { return false; }

    virtual void reloadFromDisk() {}

    virtual void persistToDisk() {}

    virtual const std::vector<AccessGrant>& grants() const = 0;

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;
};

class DefaultPolicyStore : public PolicyStore {
  public:
    bool hasAcl(const std::string& aclId) const override;

    std::optional<ACList> getAcl(const std::string& aclId) const override;

    std::vector<std::string> listAcls() const override;

    void addAcl(ACList acl) override;

    void updateAcl(ACList acl) override;

    void removeAcl(const std::string& aclId) override;

    std::vector<AccessGrant> grantsOf(const IdentityRef& identity) const override;

    void addGrant(AccessGrant grant) override;

    void removeGrant(const AccessGrant& grant) override;

    std::vector<PolicyBinding> bindingsOf(const IdentityRef& identity) const override;

    void addBinding(PolicyBinding binding) override;

    void removeBinding(const PolicyBinding& binding) override;

    std::vector<ACEntry> effectiveEntriesOf(const IdentityRef& identity) const override;

    void clear() override;

    const std::vector<AccessGrant>& grants() const override { return m_grants; }

    const std::vector<ACList>& acls() const { return m_acls; }

    const std::vector<PolicyBinding>& bindings() const { return m_bindings; }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts) override;

    void jsonOut(boost::json::object& o, const JsonFormOptions& opts) override;

  private:
    std::vector<ACList> m_acls;
    std::vector<AccessGrant> m_grants;
    std::vector<PolicyBinding> m_bindings;
};

class DecoratedPolicyStore : public PolicyStore {
  public:
    explicit DecoratedPolicyStore(std::shared_ptr<PolicyStore> wrapped)
        : m_wrapped(std::move(wrapped)) {}

    bool hasAcl(const std::string& aclId) const override { return m_wrapped->hasAcl(aclId); }
    std::optional<ACList> getAcl(const std::string& aclId) const override {
        return m_wrapped->getAcl(aclId);
    }
    std::vector<std::string> listAcls() const override { return m_wrapped->listAcls(); }
    void addAcl(ACList acl) override { m_wrapped->addAcl(std::move(acl)); }
    void updateAcl(ACList acl) override { m_wrapped->updateAcl(std::move(acl)); }
    void removeAcl(const std::string& aclId) override { m_wrapped->removeAcl(aclId); }
    std::vector<AccessGrant> grantsOf(const IdentityRef& identity) const override {
        return m_wrapped->grantsOf(identity);
    }
    void addGrant(AccessGrant grant) override { m_wrapped->addGrant(std::move(grant)); }
    void removeGrant(const AccessGrant& grant) override { m_wrapped->removeGrant(grant); }
    std::vector<PolicyBinding> bindingsOf(const IdentityRef& identity) const override {
        return m_wrapped->bindingsOf(identity);
    }
    void addBinding(PolicyBinding binding) override { m_wrapped->addBinding(std::move(binding)); }
    void removeBinding(const PolicyBinding& binding) override { m_wrapped->removeBinding(binding); }
    std::vector<ACEntry> effectiveEntriesOf(const IdentityRef& identity) const override {
        return m_wrapped->effectiveEntriesOf(identity);
    }
    void clear() override { m_wrapped->clear(); }
    const std::vector<AccessGrant>& grants() const override { return m_wrapped->grants(); }

    void jsonIn(const boost::json::object& o, const JsonFormOptions& opts) override {
        m_wrapped->jsonIn(o, opts);
    }

    void jsonOut(boost::json::object& o, const JsonFormOptions& opts) override {
        m_wrapped->jsonOut(o, opts);
    }

    int invoke(std::vector<std::string>& args) override { return m_wrapped->invoke(args); }
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override {
        return m_wrapped->complete(args, index);
    }

    PolicyStore* wrapped() { return m_wrapped.get(); }
    const PolicyStore* wrapped() const { return m_wrapped.get(); }

  protected:
    std::shared_ptr<PolicyStore> m_wrapped;
};

class FilePolicyStore : public DecoratedPolicyStore {
  public:
    explicit FilePolicyStore(VolumeFile file, std::shared_ptr<PolicyStore> wrapped);

    explicit FilePolicyStore(std::filesystem::path path, std::shared_ptr<PolicyStore> wrapped);

    std::filesystem::path storePath() const override;

    bool canPersistToDisk() const override;

    void reloadFromDisk() override;

    void persistToDisk() override;

    void addGrant(AccessGrant grant) override;

    void removeGrant(const AccessGrant& grant) override;

    void clear() override;

    void addAcl(ACList acl) override;

    void removeAcl(const std::string& aclId) override;

    void addBinding(PolicyBinding binding) override;

    void removeBinding(const PolicyBinding& binding) override;

    const VolumeFile& file() const { return m_file; }

  private:
    void loadFromDisk();
    void saveToDisk();

    std::shared_ptr<LocalVolume> m_ownedVolume;
    VolumeFile m_file;
};

std::vector<ACEMatch> policyFind(const PolicyStore& store, const IdentityRef& identity,
                                const Permission& permission, const PermissionMatcher& matcher);

AccessEffect policyCheck(const PolicyStore& store, const IdentityRef& identity,
                         const Permission& permission, const PermissionMatcher& matcher,
                         const ACResolvePolicy& resolver);

AccessEffect policyCheckAny(const PolicyStore& store, const std::vector<Identity>& identities,
                            const Permission& permission, const PermissionMatcher& matcher,
                            const ACResolvePolicy& resolver);

} // namespace bas::security

#endif
