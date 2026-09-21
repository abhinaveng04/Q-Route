#pragma once

#include "qroute/common/types.hpp"

namespace qroute::unit1 {

// Cache rationale (must appear in code comments):
// ASTNode (24 B) packs ~2.6 nodes per 64-byte cache line; traversal touches only left, right, leftthread, rightthread. The 64-byte GatePayload is dereferenced only when a visitor executes the instruction, preventing cache pollution during scans.
struct ASTNode {
    uint32_t left    = kNil;    // child index OR thread index
    uint32_t right   = kNil;    // child index OR thread index
    uint32_t payload = kNil;    // index into cold payload array
    uint32_t seq     = 0;       // source-order sequence number
    union {
        uint32_t source_line = 0;
        uint32_t sourceline;
    };
    OpCode   op      = OpCode::Nop;
    union {
        uint8_t left_thread  = 0;   // 1 => left is inorder predecessor
        uint8_t leftthread;
    };
    union {
        uint8_t right_thread = 0;   // 1 => right is inorder successor
        uint8_t rightthread;
    };
    uint8_t  reserved = 0;
};

static_assert(sizeof(ASTNode) == 24, "ASTNode must be exactly 24 bytes (cache-line packing contract)");
static_assert(alignof(ASTNode) == 4, "ASTNode must not be over-aligned");

} // namespace qroute::unit1