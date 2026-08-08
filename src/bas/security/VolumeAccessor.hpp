#ifndef BAS_SECURITY_VOLUME_ACCESSOR_HPP
#define BAS_SECURITY_VOLUME_ACCESSOR_HPP

#include "Permission.hpp"
#include "Realm.hpp"
#include "Subject.hpp"
#include "Types.hpp"

#include "../volume/Volume.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace bas::security {

class SecurityManager;
class UserStore;
class PolicyStore;

/** Build realm slot for a volume: type=volume, uuid=UUID (else name=url). */
Realm realmForVolume(const Volume* volume);

/**
 * Guarded volume: every mutating / content / listing op is checked via the volume's
 * PolicyStore and the SecurityManager session (or a one-shot elevation subject).
 *
 * Metadata probes (exists / isFile / isDirectory / stat) are not gated.
 */
class VolumeAccessor : public Volume {
  public:
    VolumeAccessor(Volume* inner, SecurityManager* securityManager);

    Volume* inner() const { return m_inner; }
    SecurityManager* securityManager() const { return m_sm; }
    const Realm& realm() const { return m_realm; }

    /** One-shot elevation subject for the next checks (cleared manually or after clearElevation). */
    void setElevationSubject(std::optional<Subject> subject);
    void clearElevation();
    const std::optional<Subject>& elevationSubject() const { return m_elevation; }

    /** Check without throwing. */
    AccessEffect check(const Permission& permission) const;

    /** checkPermission; throws AccessDenied on deny. */
    void require(const Permission& permission) const;

    std::shared_ptr<UserStore> getUserStore() override;
    std::shared_ptr<PolicyStore> getPolicyStore() override;

    std::string getClass() const override;
    std::string getUrl() const override;
    std::string getDeviceUrl() const override;
    VolumeType getType() const override;
    
    std::string readUuid() override;
    std::string readLabel() override;
    bool writeUuid(std::string_view uuid) override;
    bool writeLabel(std::string_view label) override;

    bool isEncrypted() const override;
    bool isLocal() const override;
    std::optional<std::string> getLocalFile(std::string_view path) const override;

    bool exists(std::string_view path) const override;
    bool isFile(std::string_view path) const override;
    bool isDirectory(std::string_view path) const override;
    bool stat(std::string_view path, DirNode* status) const override;

    void readDir_inplace(DirNode& context, std::string_view path, bool recursive = false) override;

    std::unique_ptr<InputStream> newInputStream(std::string_view path) override;
    std::unique_ptr<OutputStream> newOutputStream(std::string_view path,
                                                  bool append = false) override;
    std::unique_ptr<RandomInputStream> newRandomInputStream(std::string_view path) override;

    std::string getTempDir() override;
    std::string createTempFile(std::string_view prefix = "tmp.",
                               std::string_view suffix = "") override;

  protected:
    std::string getDefaultLabel() const override;

    std::vector<uint8_t> readFileUnchecked(std::string_view path, int64_t off = 0,
                                           size_t len = 0) override;
    void writeFileUnchecked(std::string_view path, const std::vector<uint8_t>& data) override;
    void createDirectoryThrowsUnchecked(std::string_view path) override;
    void removeDirectoryThrowsUnchecked(std::string_view path) override;
    void removeFileThrowsUnchecked(std::string_view path) override;
    void copyFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void moveFileThrowsUnchecked(std::string_view src, std::string_view dest) override;
    void renameFileThrowsUnchecked(std::string_view src, std::string_view dest) override;

  private:
    Permission perm(std::string_view action, std::string_view resource) const;
    std::vector<Identity> authorizationIdentities() const;

    Volume* m_inner{nullptr};
    SecurityManager* m_sm{nullptr};
    Realm m_realm;
    std::optional<Subject> m_elevation;
};

} // namespace bas::security

#endif
