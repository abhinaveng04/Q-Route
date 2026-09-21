#pragma once

#include <cstdint>

namespace qroute::unit2 {

struct PhysicalEdge {
    uint32_t to = 0;
    uint32_t reverse_index = 0;
    double   fidelity = 1.0;
    double   weight = 0.0;       // -ln(fidelity)
    double   gate_time_ns = 0.0;
};

static_assert(sizeof(PhysicalEdge) == 32, "PhysicalEdge must be exactly 32 bytes");

} // namespace qroute::unit2
