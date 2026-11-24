#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit4_bnb/lookahead_graph.hpp"
#include "qroute/unit4_bnb/qubit_placement_bnb.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

using namespace qroute;
using namespace qroute::unit1;
using namespace qroute::unit2;
using namespace qroute::unit4;

// Brute-force reference oracle strictly inside test file (never in src/)
static double brute_force_optimal_cost(
    uint32_t n, uint32_t m,
    const PhysicalCouplingGraph& g,
    const LookaheadGraph& lg,
    bool use_hop_distance = true,
    std::vector<int32_t>* out_best_assignment = nullptr)
{
    if (n > m) return kInfinity;
    std::vector<PhysicalQubit> phys(m);
    for (PhysicalQubit i = 0; i < m; ++i) phys[i] = i;

    double best_cost = kInfinity;
    std::vector<int32_t> best_assign(n, -1);

    do {
        double cost = 0.0;
        bool unreachable = false;
        for (const auto& [i, j] : lg.edges()) {
            uint32_t w = lg.weight(i, j);
            if (w == 0) continue;
            double d = 0.0;
            if (use_hop_distance) {
                uint16_t h = g.hops(phys[i], phys[j]);
                if (h == std::numeric_limits<uint16_t>::max()) {
                    unreachable = true;
                    break;
                }
                d = static_cast<double>(h);
            } else {
                d = g.weighted_distance(phys[i], phys[j]);
                if (d >= kInfinity) {
                    unreachable = true;
                    break;
                }
            }
            cost += static_cast<double>(w) * d;
        }

        if (!unreachable && cost < best_cost) {
            best_cost = cost;
            for (uint32_t k = 0; k < n; ++k) {
                best_assign[k] = static_cast<int32_t>(phys[k]);
            }
        }
    } while (std::next_permutation(phys.begin(), phys.end()));

    if (out_best_assignment) {
        *out_best_assignment = best_assign;
    }
    return best_cost;
}

// T4.1: LookaheadGraph build: alpha_ij counts, symmetry, degree ordering determinism
TEST(Unit4BnB, T4_1_LookaheadGraph_Build) {
    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");
    ASSERT_EQ(prog.total_logical_qubits, 5U);

    LookaheadGraph lg(prog.total_logical_qubits);
    lg.build(prog.ast);

    ASSERT_EQ(lg.logical_count(), 5U);

    // Verify interaction counts from circuit.qasm
    // cx q[0],q[4]; cx q[1],q[3]; cx q[0],q[3]; cx q[1],q[4];
    ASSERT_EQ(lg.weight(0, 4), 1U);
    ASSERT_EQ(lg.weight(4, 0), 1U);
    ASSERT_EQ(lg.weight(1, 3), 1U);
    ASSERT_EQ(lg.weight(3, 1), 1U);
    ASSERT_EQ(lg.weight(0, 3), 1U);
    ASSERT_EQ(lg.weight(3, 0), 1U);
    ASSERT_EQ(lg.weight(1, 4), 1U);
    ASSERT_EQ(lg.weight(4, 1), 1U);

    // Qubit 2 has no 2-qubit interactions
    ASSERT_EQ(lg.weight(2, 0), 0U);
    ASSERT_EQ(lg.weight(2, 1), 0U);
    ASSERT_EQ(lg.weight(2, 3), 0U);
    ASSERT_EQ(lg.weight(2, 4), 0U);

    // LG-2: Diagonal is always 0
    for (uint32_t i = 0; i < 5; ++i) {
        ASSERT_EQ(lg.weight(i, i), 0U);
    }

    // LG-2: Symmetry across all pairs
    for (uint32_t i = 0; i < 5; ++i) {
        for (uint32_t j = 0; j < 5; ++j) {
            ASSERT_EQ(lg.weight(i, j), lg.weight(j, i));
        }
    }

    // LG-3: edges() returned in ascending (i, j) order
    const auto& edges = lg.edges();
    ASSERT_EQ(edges.size(), 4ULL);
    for (size_t k = 0; k + 1 < edges.size(); ++k) {
        ASSERT_TRUE(edges[k] < edges[k + 1]);
    }
    ASSERT_EQ(edges[0].first, 0U); ASSERT_EQ(edges[0].second, 3U);
    ASSERT_EQ(edges[1].first, 0U); ASSERT_EQ(edges[1].second, 4U);
    ASSERT_EQ(edges[2].first, 1U); ASSERT_EQ(edges[2].second, 3U);
    ASSERT_EQ(edges[3].first, 1U); ASSERT_EQ(edges[3].second, 4U);

    // Degree verification
    const auto& deg = lg.degree();
    ASSERT_EQ(deg.size(), 5ULL);
    ASSERT_EQ(deg[0], 2U);
    ASSERT_EQ(deg[1], 2U);
    ASSERT_EQ(deg[2], 0U);
    ASSERT_EQ(deg[3], 2U);
    ASSERT_EQ(deg[4], 2U);

    // LG-4: degree-sorted order (degree DESC, then id ASC)
    // Degree 2: 0, 1, 3, 4; Degree 0: 2
    const auto& order = lg.degree_sorted_order();
    ASSERT_EQ(order.size(), 5ULL);
    std::vector<LogicalQubit> expected_order = {0, 1, 3, 4, 2};
    ASSERT_TRUE(order == expected_order);

    // Lookahead window test (window = 2 gates)
    LookaheadGraph lg_win(5);
    lg_win.build(prog.ast, 2);
    ASSERT_EQ(lg_win.edges().size(), 2ULL);
    ASSERT_EQ(lg_win.weight(0, 4), 1U);
    ASSERT_EQ(lg_win.weight(1, 3), 1U);
    ASSERT_EQ(lg_win.weight(0, 3), 0U);
    ASSERT_EQ(lg_win.weight(1, 4), 0U);
}

// T4.2: Greedy upper bound produces a feasible injective assignment
TEST(Unit4BnB, T4_2_GreedyUpperBound_FeasibleInjective) {
    PhysicalCouplingGraph g(5);
    for (uint32_t i = 0; i < 4; ++i) {
        g.add_undirected_edge(i, i + 1, 0.99, 300.0);
    }
    g.compute_all_pairs();

    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");
    LookaheadGraph lg(5);
    lg.build(prog.ast);

    PlacementConfig cfg;
    cfg.logical_count = 5;
    cfg.physical_count = 5;
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB bnb(cfg);
    double greedy_cost = bnb.greedy_upper_bound();
    ASSERT_LT(greedy_cost, kInfinity);

    const auto& assignment = bnb.best_assignment();
    ASSERT_EQ(assignment.size(), 5ULL);

    // Verify injectivity
    std::vector<bool> used(5, false);
    for (int32_t p : assignment) {
        ASSERT_GE(p, 0);
        ASSERT_LT(p, 5);
        ASSERT_FALSE(used[p]);
        used[p] = true;
    }

    // Verify cost consistency
    double manual_cost = 0.0;
    for (const auto& [i, j] : lg.edges()) {
        manual_cost += lg.weight(i, j) * g.hops(assignment[i], assignment[j]);
    }
    ASSERT_NEAR(greedy_cost, manual_cost, 1e-9);
}

// T4.3: B&B == brute force on (4,4), (5,4), (6,5), (6,6)
TEST(Unit4BnB, T4_3_BnB_Matches_BruteForce_AllCases) {
    // Case 1: (4, 4) - 4 logical, 4 physical
    {
        PhysicalCouplingGraph g(4);
        g.add_undirected_edge(0, 1, 0.95, 300.0);
        g.add_undirected_edge(1, 2, 0.95, 300.0);
        g.add_undirected_edge(2, 3, 0.95, 300.0);
        g.compute_all_pairs();

        LookaheadGraph lg(4);
        lg.add_interaction(0, 1, 2);
        lg.add_interaction(1, 2, 1);
        lg.add_interaction(2, 3, 3);
        lg.add_interaction(0, 3, 1);

        double bf_cost = brute_force_optimal_cost(4, 4, g, lg);

        PlacementConfig cfg;
        cfg.logical_count = 4;
        cfg.physical_count = 4;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_NEAR(res.best_cost, bf_cost, 1e-9);
    }

    // Case 2: (5, 4) - 4 logical, 5 physical (m=5, n=4)
    {
        PhysicalCouplingGraph g(5);
        g.add_undirected_edge(0, 1, 0.95, 300.0);
        g.add_undirected_edge(0, 2, 0.95, 300.0);
        g.add_undirected_edge(0, 3, 0.95, 300.0);
        g.add_undirected_edge(0, 4, 0.95, 300.0);
        g.compute_all_pairs();

        LookaheadGraph lg(4);
        lg.add_interaction(0, 1, 3);
        lg.add_interaction(0, 2, 2);
        lg.add_interaction(0, 3, 1);
        lg.add_interaction(1, 2, 2);

        double bf_cost = brute_force_optimal_cost(4, 5, g, lg);

        PlacementConfig cfg;
        cfg.logical_count = 4;
        cfg.physical_count = 5;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_NEAR(res.best_cost, bf_cost, 1e-9);
    }

    // Case 3: (6, 5) - 5 logical, 6 physical (m=6, n=5)
    {
        PhysicalCouplingGraph g(6);
        for (uint32_t i = 0; i < 5; ++i) {
            g.add_undirected_edge(i, i + 1, 0.95, 300.0);
        }
        g.add_undirected_edge(5, 0, 0.95, 300.0); // Ring of 6
        g.compute_all_pairs();

        LookaheadGraph lg(5);
        lg.add_interaction(0, 1, 3);
        lg.add_interaction(1, 2, 1);
        lg.add_interaction(2, 3, 4);
        lg.add_interaction(3, 4, 2);
        lg.add_interaction(0, 4, 1);

        double bf_cost = brute_force_optimal_cost(5, 6, g, lg);

        PlacementConfig cfg;
        cfg.logical_count = 5;
        cfg.physical_count = 6;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_NEAR(res.best_cost, bf_cost, 1e-9);
    }

    // Case 4: (6, 6) - 6 logical, 6 physical (m=6, n=6)
    {
        PhysicalCouplingGraph g(6);
        // 2x3 grid:
        // 0 - 1 - 2
        // |   |   |
        // 3 - 4 - 5
        g.add_undirected_edge(0, 1, 0.95, 300.0);
        g.add_undirected_edge(1, 2, 0.95, 300.0);
        g.add_undirected_edge(3, 4, 0.95, 300.0);
        g.add_undirected_edge(4, 5, 0.95, 300.0);
        g.add_undirected_edge(0, 3, 0.95, 300.0);
        g.add_undirected_edge(1, 4, 0.95, 300.0);
        g.add_undirected_edge(2, 5, 0.95, 300.0);
        g.compute_all_pairs();

        LookaheadGraph lg(6);
        lg.add_interaction(0, 1, 2);
        lg.add_interaction(1, 2, 3);
        lg.add_interaction(2, 3, 1);
        lg.add_interaction(3, 4, 4);
        lg.add_interaction(4, 5, 2);
        lg.add_interaction(0, 5, 1);

        double bf_cost = brute_force_optimal_cost(6, 6, g, lg);

        PlacementConfig cfg;
        cfg.logical_count = 6;
        cfg.physical_count = 6;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_NEAR(res.best_cost, bf_cost, 1e-9);
    }
}

// T4.4: Cost function matches manual computation on a hand-verified instance
TEST(Unit4BnB, T4_4_CostFunctionMatchesManualComputation) {
    // 3 physical qubits in a line: 0 - 1 - 2
    PhysicalCouplingGraph g(3);
    g.add_undirected_edge(0, 1, 0.9, 100.0);
    g.add_undirected_edge(1, 2, 0.9, 100.0);
    g.compute_all_pairs();

    // 3 logical qubits with interactions:
    // alpha(0,1) = 3, alpha(1,2) = 2, alpha(0,2) = 1
    LookaheadGraph lg(3);
    lg.add_interaction(0, 1, 3);
    lg.add_interaction(1, 2, 2);
    lg.add_interaction(0, 2, 1);

    // Hand calculation:
    // If pi = (0, 1, 2): cost = 3*d(0,1) + 2*d(1,2) + 1*d(0,2) = 3*1 + 2*1 + 1*2 = 7.0
    // If pi = (0, 2, 1): cost = 3*d(0,2) + 2*d(2,1) + 1*d(0,1) = 3*2 + 2*1 + 1*1 = 9.0
    // If pi = (1, 0, 2): cost = 3*d(1,0) + 2*d(0,2) + 1*d(1,2) = 3*1 + 2*2 + 1*1 = 8.0
    // If pi = (2, 1, 0): cost = 3*d(2,1) + 2*d(1,0) + 1*d(2,0) = 3*1 + 2*1 + 1*2 = 7.0
    // The optimal cost is 7.0.

    PlacementConfig cfg;
    cfg.logical_count = 3;
    cfg.physical_count = 3;
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB bnb(cfg);
    PlacementResult res = bnb.solve();

    ASSERT_TRUE(res.completed);
    ASSERT_TRUE(res.feasible);
    ASSERT_NEAR(res.best_cost, 7.0, 1e-9);

    // Verify the returned placement indeed achieves cost 7.0
    const auto& pi = res.logical_to_physical;
    double actual = 3.0 * g.hops(pi[0], pi[1]) + 2.0 * g.hops(pi[1], pi[2]) + 1.0 * g.hops(pi[0], pi[2]);
    ASSERT_NEAR(actual, 7.0, 1e-9);
}

// T4.5: Lower-bound admissibility verified against exhaustive completion costs
TEST(Unit4BnB, T4_5_LowerBoundAdmissibility) {
    PhysicalCouplingGraph g(4);
    g.add_undirected_edge(0, 1, 0.95, 300.0);
    g.add_undirected_edge(1, 2, 0.95, 300.0);
    g.add_undirected_edge(2, 3, 0.95, 300.0);
    g.compute_all_pairs();

    LookaheadGraph lg(4);
    lg.add_interaction(0, 1, 2);
    lg.add_interaction(1, 2, 1);
    lg.add_interaction(2, 3, 3);
    lg.add_interaction(0, 3, 1);

    PlacementConfig cfg;
    cfg.logical_count = 4;
    cfg.physical_count = 4;
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB bnb(cfg);
    bnb.precompute_bound_structures();

    // 1) Root node (depth 0, no assignments)
    double lb_root = bnb.lower_bound(0.0);
    double opt = brute_force_optimal_cost(4, 4, g, lg);
    ASSERT_LE(lb_root, opt + 1e-9);

    // 2) Depth 1: assign logical 0 to each physical qubit p, test LB <= all completions
    for (PhysicalQubit p0 = 0; p0 < 4; ++p0) {
        bnb.assign(0, p0);
        double assigned_cost = 0.0;
        double lb = bnb.lower_bound(assigned_cost);

        // Enumerate all completions for logical 1, 2, 3
        std::vector<PhysicalQubit> remaining;
        for (PhysicalQubit p = 0; p < 4; ++p) {
            if (p != p0) remaining.push_back(p);
        }

        double min_completion = kInfinity;
        do {
            PhysicalQubit p1 = remaining[0];
            PhysicalQubit p2 = remaining[1];
            PhysicalQubit p3 = remaining[2];

            double true_cost = lg.weight(0, 1) * g.hops(p0, p1) +
                               lg.weight(1, 2) * g.hops(p1, p2) +
                               lg.weight(2, 3) * g.hops(p2, p3) +
                               lg.weight(0, 3) * g.hops(p0, p3);
            if (true_cost < min_completion) min_completion = true_cost;

            // Admissibility: LB must be <= EVERY completion
            ASSERT_LE(lb, true_cost + 1e-9);
        } while (std::next_permutation(remaining.begin(), remaining.end()));

        ASSERT_LE(lb, min_completion + 1e-9);
        bnb.undo_assign(0, p0);
    }
}

// T4.6: Pruning activity: branches_pruned > 0 and nodes_expanded recorded
TEST(Unit4BnB, T4_6_PruningActivityAndNodesExpanded) {
    PhysicalCouplingGraph g(5);
    for (uint32_t i = 0; i < 4; ++i) {
        g.add_undirected_edge(i, i + 1, 0.95, 300.0);
    }
    g.compute_all_pairs();

    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");
    LookaheadGraph lg(5);
    lg.build(prog.ast);

    PlacementConfig cfg;
    cfg.logical_count = 5;
    cfg.physical_count = 5;
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB bnb(cfg);
    PlacementResult res = bnb.solve();

    ASSERT_TRUE(res.completed);
    ASSERT_TRUE(res.feasible);
    ASSERT_GT(res.branches_pruned, 0ULL);
    ASSERT_GT(res.nodes_expanded, 0ULL);
    ASSERT_GT(res.max_stack_depth, 0U);
}

// T4.7: max_stack_depth() <= n + 1 runtime assertion verified
TEST(Unit4BnB, T4_7_MaxStackDepthBound) {
    std::vector<std::pair<uint32_t, uint32_t>> test_sizes = {
        {2, 3}, {4, 4}, {5, 5}, {6, 6}
    };

    for (const auto& [n, m] : test_sizes) {
        PhysicalCouplingGraph g(m);
        for (uint32_t i = 0; i + 1 < m; ++i) {
            g.add_undirected_edge(i, i + 1, 0.95, 300.0);
        }
        g.compute_all_pairs();

        LookaheadGraph lg(n);
        for (uint32_t i = 0; i + 1 < n; ++i) {
            lg.add_interaction(i, i + 1, 1);
        }

        PlacementConfig cfg;
        cfg.logical_count = n;
        cfg.physical_count = m;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_LE(res.max_stack_depth, n + 1);
        ASSERT_LE(bnb.max_stack_depth(), n + 1);
    }
}

// T4.8: Budget enforcement: node_budget / nodes_expanded cap honored, completed = false
TEST(Unit4BnB, T4_8_BudgetEnforcement_NodeBudget) {
    PhysicalCouplingGraph g(6);
    for (uint32_t i = 0; i + 1 < 6; ++i) {
        g.add_undirected_edge(i, i + 1, 0.95, 300.0);
    }
    g.compute_all_pairs();

    LookaheadGraph lg(6);
    lg.add_interaction(0, 1, 1);
    lg.add_interaction(1, 2, 1);
    lg.add_interaction(2, 3, 1);
    lg.add_interaction(3, 4, 1);
    lg.add_interaction(4, 5, 1);

    PlacementConfig cfg;
    cfg.logical_count = 6;
    cfg.physical_count = 6;
    cfg.graph = &g;
    cfg.lookahead = lg;
    cfg.node_budget = 3; // very strict budget

    QubitPlacementBAndB bnb(cfg);
    PlacementResult res = bnb.solve();

    ASSERT_FALSE(res.completed);
    ASSERT_LE(res.nodes_expanded, 10ULL);
    // Greedy bound still yielded a feasible assignment
    ASSERT_TRUE(res.feasible);
    ASSERT_LT(res.best_cost, kInfinity);
}

// T4.9: Time limit enforcement: time_limit_ms honored, completed = false
TEST(Unit4BnB, T4_9_TimeLimitEnforcement) {
    PhysicalCouplingGraph g(6);
    for (uint32_t i = 0; i + 1 < 6; ++i) {
        g.add_undirected_edge(i, i + 1, 0.95, 300.0);
    }
    g.compute_all_pairs();

    LookaheadGraph lg(6);
    lg.add_interaction(0, 1, 1);
    lg.add_interaction(1, 2, 1);
    lg.add_interaction(2, 3, 1);
    lg.add_interaction(3, 4, 1);
    lg.add_interaction(4, 5, 1);

    PlacementConfig cfg;
    cfg.logical_count = 6;
    cfg.physical_count = 6;
    cfg.graph = &g;
    cfg.lookahead = lg;
    cfg.time_limit_ms = 0.00001; // tiny deadline to force timeout

    QubitPlacementBAndB bnb(cfg);
    PlacementResult res = bnb.solve();

    ASSERT_FALSE(res.completed);
    ASSERT_TRUE(res.feasible);
    ASSERT_LT(res.best_cost, kInfinity);
}

// T4.10: Zero-interaction or empty circuit / lookahead graph handled gracefully
TEST(Unit4BnB, T4_10_ZeroInteraction_Or_EmptyCircuit) {
    PhysicalCouplingGraph g(4);
    for (uint32_t i = 0; i + 1 < 4; ++i) {
        g.add_undirected_edge(i, i + 1, 0.95, 300.0);
    }
    g.compute_all_pairs();

    // 1) Empty circuit (n = 0)
    {
        PlacementConfig cfg;
        cfg.logical_count = 0;
        cfg.physical_count = 4;
        cfg.graph = &g;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_EQ(res.best_cost, 0.0);
        ASSERT_TRUE(res.logical_to_physical.empty());
    }

    // 2) 3 qubits, 0 interactions
    {
        LookaheadGraph lg(3);
        PlacementConfig cfg;
        cfg.logical_count = 3;
        cfg.physical_count = 4;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.completed);
        ASSERT_TRUE(res.feasible);
        ASSERT_EQ(res.best_cost, 0.0);
        ASSERT_EQ(res.logical_to_physical.size(), 3ULL);
    }
}

// T4.11: Infeasible inputs: n > m returns feasible == false without crashing
TEST(Unit4BnB, T4_11_InfeasibleInputs_N_Greater_Than_M) {
    PhysicalCouplingGraph g(3);
    g.add_undirected_edge(0, 1, 0.95, 300.0);
    g.add_undirected_edge(1, 2, 0.95, 300.0);
    g.compute_all_pairs();

    LookaheadGraph lg(5);
    lg.add_interaction(0, 1, 1);

    PlacementConfig cfg;
    cfg.logical_count = 5;
    cfg.physical_count = 3; // n > m
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB bnb(cfg);
    PlacementResult res = bnb.solve();

    ASSERT_TRUE(res.completed);
    ASSERT_FALSE(res.feasible);
    ASSERT_EQ(res.best_cost, kInfinity);
    ASSERT_EQ(res.logical_to_physical.size(), 5ULL);
    for (int32_t p : res.logical_to_physical) {
        ASSERT_EQ(p, -1);
    }
}

// T4.12: Disconnected coupling graph handled & Weighted-distance mode correctness
TEST(Unit4BnB, T4_12_DisconnectedGraph_And_WeightedDistance) {
    // 1) Disconnected graph: {0, 1} and {2, 3}
    {
        PhysicalCouplingGraph g(4);
        g.add_undirected_edge(0, 1, 0.95, 300.0);
        g.add_undirected_edge(2, 3, 0.95, 300.0);
        g.compute_all_pairs();

        // 3 logical qubits with triangle interactions: requires 3 connected qubits
        LookaheadGraph lg(3);
        lg.add_interaction(0, 1, 1);
        lg.add_interaction(1, 2, 1);
        lg.add_interaction(0, 2, 1);

        PlacementConfig cfg;
        cfg.logical_count = 3;
        cfg.physical_count = 4;
        cfg.graph = &g;
        cfg.lookahead = lg;

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_FALSE(res.feasible);
        ASSERT_EQ(res.best_cost, kInfinity);
    }

    // 2) Weighted-distance mode correctness
    {
        PhysicalCouplingGraph g(3);
        // Direct edge 0-1 has poor fidelity (high weight)
        // Path 0-2-1 has high fidelity (low weight)
        g.add_undirected_edge(0, 1, 0.50, 300.0);  // weight = -ln(0.5) ~ 0.693
        g.add_undirected_edge(0, 2, 0.99, 300.0);  // weight = -ln(0.99) ~ 0.010
        g.add_undirected_edge(2, 1, 0.99, 300.0);  // weight = -ln(0.99) ~ 0.010
        g.compute_all_pairs();

        LookaheadGraph lg(2);
        lg.add_interaction(0, 1, 1);

        PlacementConfig cfg;
        cfg.logical_count = 2;
        cfg.physical_count = 3;
        cfg.graph = &g;
        cfg.lookahead = lg;
        cfg.use_hop_distance = false; // weighted mode

        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_TRUE(res.feasible);
        double bf_cost = brute_force_optimal_cost(2, 3, g, lg, false);
        ASSERT_NEAR(res.best_cost, bf_cost, 1e-9);
        ASSERT_LT(res.best_cost, 0.1); // should prefer low weight path
    }
}

// T4.13: Determinism across repeated solves
TEST(Unit4BnB, T4_13_DeterminismAcrossRepeatedSolves) {
    PhysicalCouplingGraph g(5);
    for (uint32_t i = 0; i + 1 < 5; ++i) {
        g.add_undirected_edge(i, i + 1, 0.95, 300.0);
    }
    g.compute_all_pairs();

    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");
    LookaheadGraph lg(5);
    lg.build(prog.ast);

    PlacementConfig cfg;
    cfg.logical_count = 5;
    cfg.physical_count = 5;
    cfg.graph = &g;
    cfg.lookahead = lg;

    QubitPlacementBAndB first_bnb(cfg);
    PlacementResult first_res = first_bnb.solve();

    for (int run = 0; run < 20; ++run) {
        QubitPlacementBAndB bnb(cfg);
        PlacementResult res = bnb.solve();

        ASSERT_EQ(res.best_cost, first_res.best_cost);
        ASSERT_TRUE(res.logical_to_physical == first_res.logical_to_physical);
        ASSERT_EQ(res.nodes_expanded, first_res.nodes_expanded);
        ASSERT_EQ(res.branches_pruned, first_res.branches_pruned);
    }
}

// T4.14: INV-2 static gate: no best-first containers in unit4
TEST(Unit4BnB, T4_14_INV2_StaticGate) {
    std::vector<std::string> files = {
        "include/qroute/unit4_bnb/lookahead_graph.hpp",
        "include/qroute/unit4_bnb/qubit_placement_bnb.hpp",
        "src/unit4_bnb/lookahead_graph.cpp",
        "src/unit4_bnb/qubit_placement_bnb.cpp"
    };

    std::vector<std::string> forbidden = {
        "std::priority_" "queue",
        "Fibonacci" "Heap",
        "std::multi" "set"
    };

    for (const auto& file : files) {
        std::string content = read_text_file(file);
        for (const auto& f : forbidden) {
            ASSERT_EQ(content.find(f), std::string::npos);
        }
    }
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
