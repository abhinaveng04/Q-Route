#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace qroute {

struct SafeArithmetic {
    static constexpr uint64_t kMaxU64 = std::numeric_limits<uint64_t>::max();

    // Returns true on overflow; result saturated to kMaxU64.
    static bool multiply_u64(uint64_t a, uint64_t b, uint64_t& out) noexcept;
    static bool add_u64(uint64_t a, uint64_t b, uint64_t& out) noexcept;

    // out = base + (a*b*c), saturated. Returns true on any overflow.
    static bool saturated_triple_product_add(
        uint64_t a, uint64_t b, uint64_t c, uint64_t base, uint64_t& out) noexcept;

    // Aliases matching any underscore variations
    static bool multiplyu64(uint64_t a, uint64_t b, uint64_t& out) noexcept {
        return multiply_u64(a, b, out);
    }
    static bool addu64(uint64_t a, uint64_t b, uint64_t& out) noexcept {
        return add_u64(a, b, out);
    }
    static bool saturatedtripleproduct_add(
        uint64_t a, uint64_t b, uint64_t c, uint64_t base, uint64_t& out) noexcept {
        return saturated_triple_product_add(a, b, c, base, out);
    }

    // FNV-1a 64-bit over a byte buffer; used for determinism_hash.
    static constexpr uint64_t kFnv1aOffsetBasis = 1469598103934665603ULL;
    static uint64_t fnv1a64(const void* data, size_t len, uint64_t seed = kFnv1aOffsetBasis) noexcept;
    static uint64_t fnv1a64str(const std::string& s, uint64_t seed = kFnv1aOffsetBasis) noexcept;
    static uint64_t fnv1a_64_str(const std::string& s, uint64_t seed = kFnv1aOffsetBasis) noexcept {
        return fnv1a64str(s, seed);
    }
};

inline bool SafeArithmetic::multiply_u64(uint64_t a, uint64_t b, uint64_t& out) noexcept {
    if (a == 0 || b == 0) { out = 0; return false; }
    if (a > kMaxU64 / b)  { out = kMaxU64; return true; }
    out = a * b; return false;
}

inline bool SafeArithmetic::add_u64(uint64_t a, uint64_t b, uint64_t& out) noexcept {
    if (a > kMaxU64 - b) { out = kMaxU64; return true; }
    out = a + b; return false;
}

} // namespace qroute