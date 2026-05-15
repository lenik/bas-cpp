#ifndef OMNISHELL_CORE_REGISTRY_SERVICE_HPP
#define OMNISHELL_CORE_REGISTRY_SERVICE_HPP

#include "IRegistry.hpp"

namespace os {

/** Application-wide default registry (LocalRegistry). */
IRegistry& registry();

} // namespace os

#endif
