#ifndef BAS_SECURITY_AC_LIST_HPP
#define BAS_SECURITY_AC_LIST_HPP

#include "ac_rule.hpp"
#include "command_support.hpp"
#include "identity.hpp"
#include "permission.hpp"

#include "../util/ptr.hpp"
#include "../volume/LocalVolume.hpp"
#include "../volume/VolumeFile.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace bas::security {

class ACList : public ICommandSupport {
  public:
    virtual ~ACList() = default;

    virtual std::filesystem::path aclPath() const { return {}; }

    virtual bool canPersistToDisk() const { return false; }

    virtual void reloadFromDisk() {}

    virtual void persistToDisk() {}

    int invoke(std::vector<std::string>& args) override;

    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    virtual void add(const ACEntry& entry) = 0;
    virtual void remove(const ACEntry& entry) = 0;
    virtual void clear() = 0;

    virtual const std::vector<ACEntry>& entries() const = 0;

    std::vector<ACRule> rulesOf(const IdentityRef& identity) const;

    std::vector<ACMatch> find(const IdentityRef& identity, const Permission& permission,
                              const PermissionMatcher& matcher) const;

    ACMode check(const IdentityRef& identity, const Permission& permission,
                 const PermissionMatcher& matcher, const ACResolvePolicy& resolver) const;

    ACMode checkAny(const std::vector<Identity>& identities, const Permission& permission,
                    const PermissionMatcher& matcher, const ACResolvePolicy& resolver) const;
};

class DefaultACList : public ACList {
  public:
    void add(const ACEntry& entry) override;
    void remove(const ACEntry& entry) override;
    void clear() override;
    const std::vector<ACEntry>& entries() const override;

  private:
    std::vector<ACEntry> m_entries;
};

class DecoratedACList : public ACList {
  public:
    explicit DecoratedACList(VariantPtr<ACList> wrapped);

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    void add(const ACEntry& entry) override;
    void remove(const ACEntry& entry) override;
    void clear() override;
    const std::vector<ACEntry>& entries() const override;

    const ACList* wrapped() const { return m_wrapped.operator->(); }
    ACList* wrapped() { return m_wrapped.operator->(); }

  protected:
    VariantPtr<ACList> m_wrapped;
};

/** Persists ACL entries to a JSON file via @ref VolumeFile. */
class FileACList : public DecoratedACList {
  public:
    explicit FileACList(VolumeFile file, VariantPtr<ACList> wrapped);

    /** Convenience: owns a @ref LocalVolume rooted at @a path's parent directory. */
    explicit FileACList(std::filesystem::path path, VariantPtr<ACList> wrapped);

    void add(const ACEntry& entry) override;
    void remove(const ACEntry& entry) override;
    void clear() override;

    std::filesystem::path aclPath() const override;
    bool canPersistToDisk() const override;
    void reloadFromDisk() override;
    void persistToDisk() override;

    int invoke(std::vector<std::string>& args) override;
    std::vector<std::string> complete(const std::vector<std::string>& args,
                                      std::size_t index) const override;

    const VolumeFile& file() const { return m_file; }
    std::size_t loadedCount() const { return m_loadedCount; }
    void flush();
    void reload();

  private:
    void loadFromDisk();
    void saveToDisk();

    std::unique_ptr<LocalVolume> m_ownedVolume;
    VolumeFile m_file;
    std::size_t m_loadedCount = 0;
};

/** Fill @a acl with built-in demo rules used by acdemo. */
void populateDemoAcl(ACList& acl);

/** DefaultACList preloaded with @ref populateDemoAcl. */
class DemoACList : public DefaultACList {
  public:
    DemoACList() { populateDemoAcl(*this); }
};

} // namespace bas::security

#endif
