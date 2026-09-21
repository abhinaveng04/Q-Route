#include "qroute/common/safe_arithmetic.hpp"

namespace qroute {

bool SafeArithmetic::saturated_triple_product_add(
    uint64_t a, uint64_t b, uint64_t c, uint64_t base, uint64_t& out) noexcept
{
    bool overflow = false;
    uint64_t prod1 = 0;
    if (multiply_u64(a, b, prod1)) {
        overflow = true;
    }
    uint64_t prod2 = 0;
    if (overflow || multiply_u64(prod1, c, prod2)) {
        overflow = true;
    }
    uint64_t sum = 0;
    if (overflow || add_u64(base, prod2, sum)) {
        overflow = true;
        out = kMaxU64;
        return true;
    }
    out = sum;
    return false;
}

uint64_t SafeArithmetic::fnv1a64(const void* data, size_t len, uint64_t seed) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint64_t hash = seed;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= kFnvPrime;
    }
    return hash;
}

uint64_t SafeArithmetic::fnv1a64str(const std::string& s, uint64_t seed) noexcept {
    return fnv1a64(s.data(), s.size(), seed);
}

} // namespace qroute