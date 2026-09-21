#include "qroute/unit2_graph/swap_synthesizer.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace qroute::unit2 {

std::vector<EmittedGate> SwapSynthesizer::decompose_swap(
    const PhysicalCouplingGraph& g, PhysicalQubit x, PhysicalQubit y)
{
    assert(g.has_edge(x, y));
    double fid = g.edge_fidelity(x, y);
    double wt = g.edge_weight(x, y);
    double dur = g.edge_gate_time_ns(x, y);

    std::vector<EmittedGate> gates(3);
    // Gate 1: CX(x, y)
    gates[0].op = OpCode::CX;
    gates[0].control = x;
    gates[0].target = y;
    gates[0].fidelity = fid;
    gates[0].weight = wt;
    gates[0].duration_ns = dur;
    gates[0].is_synthetic_swap_component = true;

    // Gate 2: CX(y, x)
    gates[1].op = OpCode::CX;
    gates[1].control = y;
    gates[1].target = x;
    gates[1].fidelity = fid;
    gates[1].weight = wt;
    gates[1].duration_ns = dur;
    gates[1].is_synthetic_swap_component = true;

    // Gate 3: CX(x, y)
    gates[2].op = OpCode::CX;
    gates[2].control = x;
    gates[2].target = y;
    gates[2].fidelity = fid;
    gates[2].weight = wt;
    gates[2].duration_ns = dur;
    gates[2].is_synthetic_swap_component = true;

    return gates;
}

std::vector<std::pair<PhysicalQubit, PhysicalQubit>>
SwapSynthesizer::swap_edges_for_path(const std::vector<PhysicalQubit>& path) {
    if (path.size() <= 2) return {};
    std::vector<std::pair<PhysicalQubit, PhysicalQubit>> edges;
    edges.reserve(path.size() - 2);
    for (size_t r = 0; r + 2 < path.size(); ++r) {
        edges.push_back({path[r], path[r + 1]});
    }
    return edges;
}

RoutingPlan SwapSynthesizer::route(
    const PhysicalCouplingGraph& g,
    PlacementMap& map,
    LogicalQubit a,
    LogicalQubit b) const
{
    PhysicalQubit pa = map.physical_of(a);
    PhysicalQubit pb = map.physical_of(b);

    if (pa == kNil || pb == kNil) {
        throw RoutingError("Logical qubit not mapped in PlacementMap");
    }
    if (pa == pb) {
        throw RoutingError("Identical control and target physical qubits in route");
    }

    RoutingPlan plan;

    if (g.has_edge(pa, pb)) {
        EmittedGate direct_cx;
        direct_cx.op = OpCode::CX;
        direct_cx.control = pa;
        direct_cx.target = pb;
        direct_cx.fidelity = g.edge_fidelity(pa, pb);
        direct_cx.weight = g.edge_weight(pa, pb);
        direct_cx.duration_ns = g.edge_gate_time_ns(pa, pb);
        direct_cx.is_synthetic_swap_component = false;

        assert(g.has_edge(direct_cx.control, direct_cx.target));

        plan.gates.push_back(direct_cx);
        plan.swap_count = 0;
        plan.added_cnot_count = 0;
        plan.hop_distance = 1;
        plan.path_weight = direct_cx.weight;
        plan.path_fidelity = direct_cx.fidelity;
        return plan;
    }

    std::vector<uint32_t> path = g.shortest_path(pa, pb);
    if (path.empty()) {
        throw RoutingError("logical pair unreachable on coupling graph");
    }

    size_t d = path.size() - 1; // number of hops
    plan.hop_distance = static_cast<uint16_t>(d);

    // Move the qubit at pa toward pb along the path (d - 1 SWAPs)
    for (size_t r = 0; r + 1 < d; ++r) {
        PhysicalQubit x = path[r];
        PhysicalQubit y = path[r + 1];

        auto decomp = decompose_swap(g, x, y);
        for (const auto& gate : decomp) {
            assert(g.has_edge(gate.control, gate.target));
            plan.gates.push_back(gate);
        }
        plan.swap_edges.push_back({x, y});
        map.swap_physical(x, y);
        assert(map.validate_bijection());
    }

    // Invariant: logical qubit a now resides at path[d-1], adjacent to b at path[d]
    PhysicalQubit cur_pa = map.physical_of(a);
    PhysicalQubit cur_pb = map.physical_of(b);
    assert(g.has_edge(cur_pa, cur_pb));

    EmittedGate final_cx;
    final_cx.op = OpCode::CX;
    final_cx.control = cur_pa;
    final_cx.target = cur_pb;
    final_cx.fidelity = g.edge_fidelity(cur_pa, cur_pb);
    final_cx.weight = g.edge_weight(cur_pa, cur_pb);
    final_cx.duration_ns = g.edge_gate_time_ns(cur_pa, cur_pb);
    final_cx.is_synthetic_swap_component = false;

    assert(g.has_edge(final_cx.control, final_cx.target));
    plan.gates.push_back(final_cx);

    plan.swap_count = static_cast<uint32_t>(d - 1);
    plan.added_cnot_count = 3 * plan.swap_count;

    // Accumulate total path weight & fidelity
    double total_wt = 0.0;
    for (const auto& gate : plan.gates) {
        total_wt += gate.weight;
    }
    plan.path_weight = total_wt;
    plan.path_fidelity = std::exp(-total_wt);

    assert(plan.added_cnot_count == 3 * plan.swap_count);
    return plan;
}

} // namespace qroute::unit2
