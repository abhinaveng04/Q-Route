#pragma once

#include "qroute/common/types.hpp"

#include <array>

namespace qroute::unit1 {

// Cache rationale (must appear in code comments):
// ASTNode (24 B) packs ~2.6 nodes per 64-byte cache line; traversal touches only left, right, leftthread, rightthread. The 64-byte GatePayload is dereferenced only when a visitor executes the instruction, preventing cache pollution during scans.
struct GatePayload {                                  // COLD - semantic payload
    std::array<LogicalQubit, 4> qubits{};             // 16
    std::array<double, 4>       params{};             // 32
    std::array<ClbitIndex, 2>   clbits{};             //  8
    union {
        uint32_t source_line = 0;                     //  4
        uint32_t sourceline;
    };
    union {
        uint8_t  qubit_count = 0;                     //  1
        uint8_t  qubitcount;
    };
    union {
        uint8_t  param_count = 0;                     //  1
        uint8_t  paramcount;
    };
    union {
        uint8_t  clbit_count = 0;                     //  1
        uint8_t  clbitcount;
    };
    uint8_t  flags       = 0;                         //  1
};

static_assert(sizeof(GatePayload) == 64, "GatePayload must be exactly 64 bytes (one cache line)");

} // namespace qroute::unit1