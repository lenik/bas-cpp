#include "UUIDs.hpp"

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

namespace uuid {

std::string randomUUID(uint64_t random_seed) {
    // 1. Initialize a better random engine with the seed
    std::mt19937_64 gen(random_seed);
    std::uniform_int_distribution<uint64_t> dis(0, 0xFFFFFFFFFFFFFFFF);

    // 2. Generate two 64-bit random numbers (128 bits total)
    uint64_t high = dis(gen);
    uint64_t low = dis(gen);

    // 3. Set Version 4 (0100) and Variant (10xx) bits
    // Version 4: set the 4 bits starting at bit 48 of 'high' to 0100
    high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    // Variant: set the 2 bits starting at bit 60 of 'low' to 10
    low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    // 4. Format into canonical 8-4-4-4-12 string
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << (uint32_t)(high >> 32) << "-"
       << std::setw(4) << (uint16_t)(high >> 16) << "-" << std::setw(4) << (uint16_t)high << "-"
       << std::setw(4) << (uint16_t)(low >> 48) << "-" << std::setw(12)
       << (low & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

} // namespace uuid
