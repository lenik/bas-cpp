#include "RegistryService.hpp"

#include "LocalRegistry.hpp"

namespace os {

IRegistry& registry() {
    return LocalRegistry::instance();
}

} // namespace os
