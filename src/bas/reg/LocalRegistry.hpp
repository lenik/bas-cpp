#ifndef BAS_REG_LOCAL_REGISTRY_HPP
#define BAS_REG_LOCAL_REGISTRY_HPP

#include "VolumeRegistry.hpp"

#include <string_view>

namespace bas::reg {

class LocalRegistry : public VolumeRegistry {
  public:
    explicit LocalRegistry(std::string_view root_dir, bool autoSave = true);

    static LocalRegistry& userDefault();

  private:
    static LocalRegistry s_user_default;
};

} // namespace bas::reg

#endif
