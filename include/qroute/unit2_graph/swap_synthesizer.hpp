#pragma once

#include "qroute/common/types.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit2_graph/placement_map.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace qroute::unit2 {

class RoutingError : public std::runtime_error {
public:
    explicit RoutingError(const std::string& msg) : std::runtime_error(msg) {}
};

struct EmittedGate {
    OpCode        op = OpCode::CX;
    PhysicalQubit control = kNil;
    PhysicalQubit target  = kNil;
    double        fidelity    = 1.0;
    double        weight      = 0.0;
    double        duration_ns = 0.0;
    uint32_t      source_line = 0;
    bool          is_synthetic_swap_component = false;
};

struct RoutingPlan {
    std::vector<EmittedGate> gates; // fully decomposed: every SWAP -> 3 CX
    std::vector<std::pair<PhysicalQubit, PhysicalQubit>> swap_edges; // logical SWAP edges (for counting)
    uint32_t swap_count       = 0;
    uint32_t added_cnot_count = 0; // == 3 * swap_count
    double   path_weight      = 0.0;
    double   path_fidelity    = 1.0;
    uint16_t hop_distance     = 0;
};

class SwapSynthesizer {
public:
    // Routes logical gate (a,b). Emits SWAP decomposition + final CX. MUTATES map.
    RoutingPlan route(const PhysicalCouplingGraph& g,
                      PlacementMap& map,
                      LogicalQubit a,
                      LogicalQubit b) const;

    // Pure helper: given path p0..pd, return d-1 swap edges moving p0 toward pd.
    static std::vector<std::pair<PhysicalQubit, PhysicalQubit>>
    swap_edges_for_path(const std::vector<PhysicalQubit>& path);
    static std::vector<std::pair<PhysicalQubit, PhysicalQubit>>
    swapedgesforpath(const std::vector<PhysicalQubit>& path) {
        return swap_edges_for_path(path);
    }

    // Emits CX(x,y), CX(y,x), CX(x,y) for one logical SWAP.
    static std::vector<EmittedGate> decompose_swap(const PhysicalCouplingGraph& g,
                                                  PhysicalQubit x, PhysicalQubit y);
    static std::vector<EmittedGate> decomposeswap(const PhysicalCouplingGraph& g,
                                                 PhysicalQubit x, PhysicalQubit y) {
        return decompose_swap(g, x, y);
    }
};

} // namespace qroute::unit2
