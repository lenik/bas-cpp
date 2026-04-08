#ifndef UUIDS_HPP
#define UUIDS_HPP

#include <cstdint>
#include <string>

namespace uuid {

std::string randomUUID(uint64_t random_seed);

} // namespace uuid

#endif // UUIDS_HPP
